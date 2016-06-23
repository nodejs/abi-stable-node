/*******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * This is an rough proof of concept not intended for real world usage.
 * It is currently far from a sufficiently completed work.
 *
 *  - The API is not complete nor agreed upon.
 *  - The API is not yet analyzed for performance.
 *  - Performance is expected to suffer with the usage of opaque types currently
 *    requiring all operations to go across the DLL boundary, i.e. no inlining
 *    even for operations such as asking for the type of a value or retrieving a
 *    function callback's arguments.
 *  - The V8 implementation of the API is roughly hacked together with no
 *    attention paid to error handling or fault tolerance.
 *  - Error handling in general is not included in the API at this time.
 *
 ******************************************************************************/
#include "node_jsvmapi.h"
#include <node_object_wrap.h>

#include <vector>

typedef void napi_destruct(void*);

namespace v8impl {

//=== Conversion between V8 Isolate and napi_env ==========================

    static napi_env JsEnvFromV8Isolate(v8::Isolate* isolate) {
        return static_cast<napi_env>(isolate);
    }

    static v8::Isolate* V8IsolateFromJsEnv(napi_env e) {
        return static_cast<v8::Isolate*>(e);
    }

//=== Conversion between V8 Handles and napi_value ========================

    // This is assuming v8::Local<> will always be implemented with a single
    // pointer field so that we can pass it around as a void* (maybe we should
    // use intptr_t instead of void*)
    static_assert(sizeof(void*) == sizeof(v8::Local<v8::Value>), "v8::Local<v8::Value> is too large to store in a void*");

    static napi_value JsValueFromV8LocalValue(v8::Local<v8::Value> local) {
        // This is awkward, better way? memcpy but don't want that dependency?
        union U {
            napi_value v;
            v8::Local<v8::Value> l;
            U(v8::Local<v8::Value> _l) : l(_l) { }
        } u(local);
        return u.v;
    };

    static v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v) {
        // Likewise awkward
        union U {
            napi_value v;
            v8::Local<v8::Value> l;
            U(napi_value _v) : v(_v) { }
        } u(v);
        return u.l;
    }

    static v8::Local<v8::Function> V8LocalFunctionFromJsValue(napi_value v) {
        // Likewise awkward
        union U {
            napi_value v;
            v8::Local<v8::Function> f;
            U(napi_value _v) : v(_v) { }
        } u(v);
        return u.f;
    }

    static napi_persistent JsPersistentFromV8PersistentValue(v8::Persistent<v8::Value> *per) {
      return (napi_persistent) per;
    };

    static v8::Persistent<v8::Value>* V8PersistentValueFromJsPersistentValue(napi_persistent per) {
      return (v8::Persistent<v8::Value>*) per;
    }

//=== Conversion between V8 FunctionCallbackInfo and ===========================
//=== napi_func_cb_info ===========================================

    static napi_func_cb_info JsFunctionCallbackInfoFromV8FunctionCallbackInfo(const v8::FunctionCallbackInfo<v8::Value>* cbinfo) {
        return static_cast<napi_func_cb_info>(cbinfo);
    };

    static const v8::FunctionCallbackInfo<v8::Value>* V8FunctionCallbackInfoFromJsFunctionCallbackInfo(napi_func_cb_info cbinfo) {
        return static_cast<const v8::FunctionCallbackInfo<v8::Value>*>(cbinfo);
    };

//=== Function napi_callback wrapper ================================================

    // This function callback wrapper implementation is taken from nan
    static const int kDataIndex =           0;
    static const int kFunctionIndex =       1;
    static const int kFunctionFieldCount =  2;

    static void FunctionCallbackWrapperOld(const v8::FunctionCallbackInfo<v8::Value> &info) {
        v8::Local<v8::Object> obj = info.Data().As<v8::Object>();
        napi_callback cb = reinterpret_cast<napi_callback>(
            reinterpret_cast<intptr_t>(
                obj->GetInternalField(kFunctionIndex).As<v8::External>()->Value()));
        napi_func_cb_info cbinfo = JsFunctionCallbackInfoFromV8FunctionCallbackInfo(&info);
        cb(
            v8impl::JsEnvFromV8Isolate(info.GetIsolate()),
            cbinfo);
    }

    static void FunctionCallbackWrapper(const v8::FunctionCallbackInfo<v8::Value> &info) {
        napi_callback cb = reinterpret_cast<napi_callback>(info.Data().As<v8::External>()->Value()); 
        napi_func_cb_info cbinfo = JsFunctionCallbackInfoFromV8FunctionCallbackInfo(&info);
        cb(
            v8impl::JsEnvFromV8Isolate(info.GetIsolate()),
            cbinfo);
    }

  class ObjectWrapWrapper: public node::ObjectWrap  {
    public:
      ObjectWrapWrapper(napi_value jsObject, void* nativeObj, napi_destruct* destructor) {
        _destructor = destructor;
        _nativeObj = nativeObj;
        Wrap(V8LocalValueFromJsValue(jsObject)->ToObject());
      }

      void* getValue() {
        return _nativeObj;
      }

      static void* Unwrap(napi_value jsObject) {
        ObjectWrapWrapper* wrapper = ObjectWrap::Unwrap<ObjectWrapWrapper>(V8LocalValueFromJsValue(jsObject)->ToObject());
        return wrapper->getValue(); 
      }

      virtual ~ObjectWrapWrapper() {
        if (_destructor != nullptr) {
          _destructor(_nativeObj);
        }
      }

    private:
      napi_destruct* _destructor;
      void* _nativeObj;
  };


}  // end of namespace v8impl

napi_value napi_create_functionOld(napi_env e, napi_callback cb) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    // This code is adapted from nan

    // This parameter is unused currently but would probably be useful to
    // expose in the API for client code to provide a data object to be
    // included on callback.
    v8::Local<v8::Value> data = v8::Local<v8::Value>();

    v8::Local<v8::Function> retval;

    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::ObjectTemplate> tpl = v8::ObjectTemplate::New(isolate);
    tpl->SetInternalFieldCount(v8impl::kFunctionFieldCount);
    v8::Local<v8::Object> obj = tpl->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    obj->SetInternalField(
        v8impl::kFunctionIndex
        , v8::External::New(isolate, reinterpret_cast<void *>(cb)));
    v8::Local<v8::Value> val = v8::Local<v8::Value>::New(isolate, data);

    if (!val.IsEmpty()) {
        obj->SetInternalField(v8impl::kDataIndex, val);
    }

    retval = scope.Escape(v8::Function::New(isolate
        , v8impl::FunctionCallbackWrapperOld
        , obj));

    return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_function(napi_env e, napi_callback cb) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    v8::Local<v8::Object> retval;

    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                                                    v8::External::New(isolate, (void*) cb));

    retval = scope.Escape(tpl->GetFunction());
    return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_constructor_for_wrap(napi_env e, napi_callback cb) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    v8::Local<v8::Object> retval;

    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                                                    v8::External::New(isolate, (void*) cb));

    // we need an internal field to stash the wrapped object
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    retval = scope.Escape(tpl->GetFunction());
    return v8impl::JsValueFromV8LocalValue(retval);
}


void napi_set_function_name(napi_env e, napi_value func, napi_value name) {
    v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
    v8func->SetName(v8impl::V8LocalValueFromJsValue(name).As<v8::String>());
}

void napi_set_return_value(napi_env e, napi_func_cb_info cbinfo, napi_value v) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    info->GetReturnValue().Set(val);
}

propertyname napi_proterty_name(napi_env e, const char* utf8name) {
    v8::Local<v8::String> namestring = v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), utf8name, v8::NewStringType::kInternalized).ToLocalChecked();
    return static_cast<propertyname>(v8impl::JsValueFromV8LocalValue(namestring));
}

void napi_set_property(napi_env e, napi_value o, propertyname k, napi_value v) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsValue(k);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

    obj->Set(key, val);

    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
}

napi_value napi_get_property(napi_env e, napi_value o, propertyname k) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsValue(k);
    v8::Local<v8::Value> val = obj->Get(key);
    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
   return v8impl::JsValueFromV8LocalValue(val);
}

napi_value napi_get_prototype(napi_env e, napi_value o) {
   v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
   v8::Local<v8::Value> val = obj->GetPrototype();
   return v8impl::JsValueFromV8LocalValue(val);
}

napi_value napi_create_object(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Object::New(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_create_string(napi_env e, const char* s) {
    return v8impl::JsValueFromV8LocalValue(
        v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s));
}

napi_value napi_create_number(napi_env e, double v) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));
}

napi_value napi_create_type_error(napi_env, napi_value msg) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Exception::TypeError(
            v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

napi_valuetype napi_get_type_of_value(napi_env e, napi_value vv) {
    v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(vv);

    if (v->IsNumber()) {
        return napi_valuetype::Number;
    }
    else if (v->IsString()) {
        return napi_valuetype::String;
    }
    else if (v->IsFunction()) {
        // This test has to come before IsObject because IsFunction
        // implies IsObject
        return napi_valuetype::Function;
    }
    else if (v->IsObject()) {
        return napi_valuetype::Object;
    }
    else if (v->IsBoolean()) {
        return napi_valuetype::Boolean;
    }
    else if (v->IsUndefined()) {
        return napi_valuetype::Undefined;
    }
    else if (v->IsSymbol()) {
        return napi_valuetype::Symbol;
    }
    else if (v->IsNull()) {
        return napi_valuetype::Null;
    }
    else {
        return napi_valuetype::Object; // Is this correct?
    }
}

napi_value napi_get_undefined_(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Undefined(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_null(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Null(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_false(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::False(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_true(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::True(v8impl::V8IsolateFromJsEnv(e)));
}

int napi_get_cb_args_length(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return info->Length();
}

bool napi_is_construct_call(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return info->IsConstructCall();
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
void napi_get_cb_args(napi_env e, napi_func_cb_info cbinfo, napi_value* buffer, size_t bufferlength) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);

    int i = 0;
    // size_t appropriate for the buffer length parameter?
    // Probably this API is not the way to go.
    int min = (int)bufferlength < info->Length() ? (int)bufferlength : info->Length();

    for (; i < min; i += 1) {
        buffer[i] = v8impl::JsValueFromV8LocalValue((*info)[i]);
    }

    if (i < (int)bufferlength) {
        napi_value undefined = v8impl::JsValueFromV8LocalValue(v8::Undefined(v8::Isolate::GetCurrent()));
        for (; i < (int)bufferlength; i += 1) {
            buffer[i] = undefined;
        }
    }
}

napi_value napi_get_cb_object(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return v8impl::JsValueFromV8LocalValue(info->This());
}

napi_value napi_call_function(napi_env e, napi_value scope, napi_value func, int argc, napi_value* argv) {
    std::vector<v8::Handle<v8::Value>> args(argc);

    v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
    v8::Handle<v8::Object> v8scope = v8impl::V8LocalValueFromJsValue(scope)->ToObject();
    for (int i = 0; i < argc; i++) {
      args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
    }
    v8::Handle<v8::Value> result = v8func->Call(v8scope, argc, args.data());
    return v8impl::JsValueFromV8LocalValue(result);
}

napi_value napi_get_global_scope(napi_env e) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    return v8impl::JsValueFromV8LocalValue(context->Global());
}

void napi_throw_error(napi_env e, napi_value error) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    isolate->ThrowException(
        v8impl::V8LocalValueFromJsValue(error));
    // any VM calls after this point and before returning
    // to the javascript invoker will fail
}

double napi_get_number_from_value(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->NumberValue();
}

void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj, napi_destruct* destructor) {
  new v8impl::ObjectWrapWrapper(jsObject, nativeObj, destructor);
};

void* napi_unwrap(napi_env e, napi_value jsObject) {
  return v8impl::ObjectWrapWrapper::Unwrap(jsObject);
};

napi_persistent napi_create_persistent(napi_env e, napi_value v) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent = new v8::Persistent<v8::Value>(isolate, v8impl::V8LocalValueFromJsValue(v));
  return v8impl::JsPersistentFromV8PersistentValue(thePersistent);
}

napi_value napi_get_persistent_value(napi_env e, napi_persistent p) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent = v8impl::V8PersistentValueFromJsPersistentValue(p);
  v8::Local<v8::Value> napi_value = v8::Local<v8::Value>::New(isolate, *thePersistent);
  return v8impl::JsValueFromV8LocalValue(napi_value);
};


napi_value napi_new_instance(napi_env e, napi_value cons, int argc, napi_value *argv){
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Function> v8cons = v8impl::V8LocalFunctionFromJsValue(cons);
  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Handle<v8::Value> result = v8cons->NewInstance(isolate->GetCurrentContext(), argc, args.data()).ToLocalChecked();
  return v8impl::JsValueFromV8LocalValue(result);
};


///////////////////////////////////////////
// WILL GO AWAY
///////////////////////////////////////////
void WorkaroundNewModuleInit(v8::Local<v8::Object> exports, v8::Local<v8::Object> module, workaround_init_napi_callback init) {
    init(
        v8impl::JsEnvFromV8Isolate(v8::Isolate::GetCurrent()),
        v8impl::JsValueFromV8LocalValue(exports),
        v8impl::JsValueFromV8LocalValue(module));
}

v8::Local<v8::Value> V8LocalValue(napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v);
}

napi_value JsValue(v8::Local<v8::Value> v) {
    return v8impl::JsValueFromV8LocalValue(v);
}

