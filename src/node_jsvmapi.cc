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

#include "node.h"
#include "node_jsvmapi_internal.h"
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <vector>

typedef void napi_destruct(void* v);

namespace v8impl {

//=== Conversion between V8 Isolate and napi_env ==========================

    napi_env JsEnvFromV8Isolate(v8::Isolate* isolate) {
        return reinterpret_cast<napi_env>(isolate);
    }

    v8::Isolate* V8IsolateFromJsEnv(napi_env e) {
        return reinterpret_cast<v8::Isolate*>(e);
    }

    class HandleScopeWrapper {
    public:
        explicit HandleScopeWrapper(
                     v8::Isolate* isolate) : scope(isolate) { }
    private:
        v8::HandleScope scope;
    };

    class EscapableHandleScopeWrapper {
    public:
        explicit EscapableHandleScopeWrapper(
                     v8::Isolate* isolate) : scope(isolate) { }
        template <typename T>
        v8::Local<T> Escape(v8::Local<T> handle) {
            return scope.Escape(handle);
        }
    private:
        v8::EscapableHandleScope scope;
    };

    napi_handle_scope JsHandleScopeFromV8HandleScope(HandleScopeWrapper* s) {
        return reinterpret_cast<napi_handle_scope>(s);
    }

    HandleScopeWrapper* V8HandleScopeFromJsHandleScope(napi_handle_scope s) {
        return reinterpret_cast<HandleScopeWrapper*>(s);
    }

    napi_escapable_handle_scope
    JsEscapableHandleScopeFromV8EscapableHandleScope(
                                            EscapableHandleScopeWrapper* s) {
        return reinterpret_cast<napi_escapable_handle_scope>(s);
    }

    EscapableHandleScopeWrapper*
    V8EscapableHandleScopeFromJsEscapableHandleScope(
                                             napi_escapable_handle_scope s) {
        return reinterpret_cast<EscapableHandleScopeWrapper*>(s);
    }

//=== Conversion between V8 Handles and napi_value ========================

    // This is assuming v8::Local<> will always be implemented with a single
    // pointer field so that we can pass it around as a void* (maybe we should
    // use intptr_t instead of void*)

    napi_value JsValueFromV8LocalValue(v8::Local<v8::Value> local) {
        // This is awkward, better way? memcpy but don't want that dependency?
        union U {
            napi_value v;
            v8::Local<v8::Value> l;
            U(v8::Local<v8::Value> _l) : l(_l) { }
        } u(local);
        assert(sizeof(u.v) == sizeof(u.l));
        return u.v;
    };

    v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v) {
        // Likewise awkward
        union U {
            napi_value v;
            v8::Local<v8::Value> l;
            U(napi_value _v) : v(_v) { }
        } u(v);
        assert(sizeof(u.v) == sizeof(u.l));
        return u.l;
    }

    v8::Local<v8::Value> V8LocalValueFromJsPropertyName(napi_propertyname pn) {
        // Likewise awkward
        union U {
            napi_propertyname pn;
            v8::Local<v8::Value> l;
            U(napi_propertyname _pn) : pn(_pn) { }
        } u(pn);
        assert(sizeof(u.pn) == sizeof(u.l));
        return u.l;
    }

    static v8::Local<v8::Function> V8LocalFunctionFromJsValue(napi_value v) {
        // Likewise awkward
        union U {
            napi_value v;
            v8::Local<v8::Function> f;
            U(napi_value _v) : v(_v) { }
        } u(v);
        assert(sizeof(u.v) == sizeof(u.f));
        return u.f;
    }

    static napi_persistent JsPersistentFromV8PersistentValue(
                                            v8::Persistent<v8::Value> *per) {
      return (napi_persistent) per;
    };

    static v8::Persistent<v8::Value>* V8PersistentValueFromJsPersistentValue(
                                                       napi_persistent per) {
      return (v8::Persistent<v8::Value>*) per;
    }

//=== Conversion between V8 FunctionCallbackInfo and ===========================
//=== napi_func_cb_info ===========================================

    static napi_func_cb_info JsFunctionCallbackInfoFromV8FunctionCallbackInfo(
                          const v8::FunctionCallbackInfo<v8::Value>* cbinfo) {
        return reinterpret_cast<napi_func_cb_info>(cbinfo);
    };

    static const v8::FunctionCallbackInfo<v8::Value>*
    V8FunctionCallbackInfoFromJsFunctionCallbackInfo(napi_func_cb_info info) {
        return
          reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(info);
    };

//=== Function napi_callback wrapper ==========================================

    // This function callback wrapper implementation is taken from nan
    static const int kDataIndex =           0;
    static const int kFunctionIndex =       1;
    static const int kFunctionFieldCount =  2;

    static void FunctionCallbackWrapperOld(
                             const v8::FunctionCallbackInfo<v8::Value> &info) {
        v8::Local<v8::Object> obj = info.Data().As<v8::Object>();
        napi_callback cb = reinterpret_cast<napi_callback>(
            reinterpret_cast<intptr_t>(obj->GetInternalField(kFunctionIndex)
                                              .As<v8::External>()->Value()));
        napi_func_cb_info cbinfo =
            JsFunctionCallbackInfoFromV8FunctionCallbackInfo(&info);
        cb(
            v8impl::JsEnvFromV8Isolate(info.GetIsolate()),
            cbinfo);
    }

    static void FunctionCallbackWrapper(
                             const v8::FunctionCallbackInfo<v8::Value> &info) {
        napi_callback cb = reinterpret_cast<napi_callback>(
                               info.Data().As<v8::External>()->Value());
        napi_func_cb_info cbinfo =
            JsFunctionCallbackInfoFromV8FunctionCallbackInfo(&info);
        cb(
            v8impl::JsEnvFromV8Isolate(info.GetIsolate()),
            cbinfo);
    }

  class ObjectWrapWrapper: public node::ObjectWrap  {
    public:
      ObjectWrapWrapper(napi_value jsObject, void* nativeObj,
                        napi_destruct* destructor) {
        _destructor = destructor;
        _nativeObj = nativeObj;
        Wrap(V8LocalValueFromJsValue(jsObject)->ToObject());
      }

      void* getValue() {
        return _nativeObj;
      }

      static void* Unwrap(napi_value jsObject) {
        ObjectWrapWrapper* wrapper =
            ObjectWrap::Unwrap<ObjectWrapWrapper>(
                 V8LocalValueFromJsValue(jsObject)->ToObject());
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

napi_env napi_get_current_env() {
  return v8impl::JsEnvFromV8Isolate(v8::Isolate::GetCurrent());
}

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
    v8::Local<v8::Object> obj =
        tpl->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    obj->SetInternalField(
        v8impl::kFunctionIndex,
            v8::External::New(isolate, reinterpret_cast<void *>(cb)));
    v8::Local<v8::Value> val = v8::Local<v8::Value>::New(isolate, data);

    if (!val.IsEmpty()) {
        obj->SetInternalField(v8impl::kDataIndex, val);
    }

    retval = scope.Escape(v8::Function::New(isolate,
        v8impl::FunctionCallbackWrapperOld,
        obj));

    return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_function(napi_env e, napi_callback cb) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> tpl =
      v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                v8::External::New(isolate,
                                                  reinterpret_cast<void*>(cb)));

  retval = scope.Escape(tpl->GetFunction());
  return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_constructor_for_wrap(napi_env e, napi_callback cb) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> tpl =
      v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                v8::External::New(isolate,
                                                  reinterpret_cast<void*>(cb)));

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  retval = scope.Escape(tpl->GetFunction());
  return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_constructor_for_wrap_with_methods(
    napi_env e,
    napi_callback cb,
    char* utf8name,
    int methodcount,
    napi_method_descriptor* methods) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> tpl =
      v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                v8::External::New(isolate,
                                                  reinterpret_cast<void*>(cb)));

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::String> namestring =
      v8::String::NewFromUtf8(isolate, utf8name,
          v8::NewStringType::kInternalized).ToLocalChecked();
  tpl->SetClassName(namestring);

  for (int i = 0; i < methodcount; i++) {
    v8::Local<v8::FunctionTemplate> t =
        v8::FunctionTemplate::New(isolate, v8impl::FunctionCallbackWrapper,
                                  v8::External::New(isolate,
                                  reinterpret_cast<void*>(methods[i].callback)),
                                  v8::Signature::New(isolate, tpl));
    v8::Local<v8::String> fn_name =
        v8::String::NewFromUtf8(isolate, methods[i].utf8name,
            v8::NewStringType::kInternalized).ToLocalChecked();
    tpl->PrototypeTemplate()->Set(fn_name, t);
    t->SetClassName(fn_name);
  }

  retval = scope.Escape(tpl->GetFunction());
  return v8impl::JsValueFromV8LocalValue(retval);
}

void napi_set_function_name(napi_env e, napi_value func,
                            napi_propertyname name) {
    v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
    v8func->SetName(
        v8impl::V8LocalValueFromJsPropertyName(name).As<v8::String>());
}

void napi_set_return_value(napi_env e,
                           napi_func_cb_info cbinfo, napi_value v) {
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    info->GetReturnValue().Set(val);
}

napi_propertyname napi_property_name(napi_env e, const char* utf8name) {
    v8::Local<v8::String> namestring =
        v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), utf8name,
            v8::NewStringType::kInternalized).ToLocalChecked();
    return reinterpret_cast<napi_propertyname>(
        v8impl::JsValueFromV8LocalValue(namestring));
}

napi_value napi_get_propertynames(napi_env e, napi_value o) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Array> array = obj->GetPropertyNames();
    return v8impl::JsValueFromV8LocalValue(array);
}

void napi_set_property(napi_env e, napi_value o,
                       napi_propertyname k, napi_value v) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

    obj->Set(key, val);

    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
}

bool napi_has_property(napi_env e, napi_value o, napi_propertyname k) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);

    return obj->Has(key);
}

napi_value napi_get_property(napi_env e, napi_value o, napi_propertyname k) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);
    v8::Local<v8::Value> val = obj->Get(key);
    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
    return v8impl::JsValueFromV8LocalValue(val);
}

void napi_set_element(napi_env e, napi_value o, uint32_t i, napi_value v) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

    obj->Set(i, val);

    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
}

bool napi_has_element(napi_env e, napi_value o, uint32_t i) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();

    return obj->Has(i);
}

napi_value napi_get_element(napi_env e, napi_value o, uint32_t i) {
    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> val = obj->Get(i);
    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
    return v8impl::JsValueFromV8LocalValue(val);
}

bool napi_is_array(napi_env e, napi_value v) {
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    return val->IsArray();
}

uint32_t napi_get_array_length(napi_env e, napi_value v) {
    v8::Local<v8::Array> arr =
        v8impl::V8LocalValueFromJsValue(v).As<v8::Array>();
    return arr->Length();
}

bool napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs) {
    v8::Local<v8::Value> a = v8impl::V8LocalValueFromJsValue(lhs);
    v8::Local<v8::Value> b = v8impl::V8LocalValueFromJsValue(rhs);
    return a->StrictEquals(b);
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

napi_value napi_create_array(napi_env e) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Array::New(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_create_array_with_length(napi_env e, int length) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Array::New(v8impl::V8IsolateFromJsEnv(e), length));
}

napi_value napi_create_string(napi_env e, const char* s) {
    return v8impl::JsValueFromV8LocalValue(
        v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s));
}

napi_value napi_create_string_with_length(napi_env e,
                                          const char* s, size_t length) {
    return v8impl::JsValueFromV8LocalValue(
        v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s,
            v8::NewStringType::kNormal,
            static_cast<int>(length)).ToLocalChecked());
}

napi_value napi_create_number(napi_env e, double v) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));
}

napi_value napi_create_boolean(napi_env e, bool b) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Boolean::New(v8impl::V8IsolateFromJsEnv(e), b));
}

napi_value napi_create_symbol(napi_env e, const char* s) {
    v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
    if (s == NULL) {
        return v8impl::JsValueFromV8LocalValue(v8::Symbol::New(isolate));
    } else {
        v8::Local<v8::String> string = v8::String::NewFromUtf8(isolate, s);
        return v8impl::JsValueFromV8LocalValue(
            v8::Symbol::New(isolate, string));
    }
}

napi_value napi_create_error(napi_env, napi_value msg) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Exception::Error(
            v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

napi_value napi_create_type_error(napi_env, napi_value msg) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Exception::TypeError(
            v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

napi_valuetype napi_get_type_of_value(napi_env e, napi_value vv) {
    v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(vv);

    if (v->IsNumber()) {
        return napi_number;
    } else if (v->IsString()) {
        return napi_string;
    } else if (v->IsFunction()) {
        // This test has to come before IsObject because IsFunction
        // implies IsObject
        return napi_function;
    } else if (v->IsObject()) {
        return napi_object;
    } else if (v->IsBoolean()) {
        return napi_boolean;
    } else if (v->IsUndefined()) {
        return napi_undefined;
    } else if (v->IsSymbol()) {
        return napi_symbol;
    } else if (v->IsNull()) {
        return napi_null;
    } else {
        return napi_object;   // Is this correct?
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
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return info->Length();
}

bool napi_is_construct_call(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return info->IsConstructCall();
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
void napi_get_cb_args(napi_env e, napi_func_cb_info cbinfo,
                      napi_value* buffer, size_t bufferlength) {
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);

    int i = 0;
    // size_t appropriate for the buffer length parameter?
    // Probably this API is not the way to go.
    int min =
        static_cast<int>(bufferlength) < info->Length() ?
        static_cast<int>(bufferlength) : info->Length();

    for (; i < min; i += 1) {
        buffer[i] = v8impl::JsValueFromV8LocalValue((*info)[i]);
    }

    if (i < static_cast<int>(bufferlength)) {
        napi_value undefined = v8impl::JsValueFromV8LocalValue(
            v8::Undefined(v8::Isolate::GetCurrent()));
        for (; i < static_cast<int>(bufferlength); i += 1) {
            buffer[i] = undefined;
        }
    }
}

napi_value napi_get_cb_this(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return v8impl::JsValueFromV8LocalValue(info->This());
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_value napi_get_cb_holder(napi_env e, napi_func_cb_info cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info =
        v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return v8impl::JsValueFromV8LocalValue(info->Holder());
}

napi_value napi_call_function(napi_env e, napi_value scope,
                              napi_value func, int argc, napi_value* argv) {
    std::vector<v8::Handle<v8::Value>> args(argc);

    v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
    v8::Handle<v8::Object> v8scope =
        v8impl::V8LocalValueFromJsValue(scope)->ToObject();
    for (int i = 0; i < argc; i++) {
      args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
    }
    v8::Handle<v8::Value> result = v8func->Call(v8scope, argc, args.data());
    return v8impl::JsValueFromV8LocalValue(result);
}

napi_value napi_get_global_scope(napi_env e) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    // TODO(ianhall): what if we need the global object from a different
    // context in the same isolate?
    // Should napi_env be the current context rather than the current isolate?
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    return v8impl::JsValueFromV8LocalValue(context->Global());
}

void napi_throw(napi_env e, napi_value error) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    isolate->ThrowException(
        v8impl::V8LocalValueFromJsValue(error));
    // any VM calls after this point and before returning
    // to the javascript invoker will fail
}

void napi_throw_error(napi_env e, char* msg) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    isolate->ThrowException(
      v8::Exception::Error(v8::String::NewFromUtf8(isolate, msg)));
    // any VM calls after this point and before returning
    // to the javascript invoker will fail
}

void napi_throw_type_error(napi_env e, char* msg) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    isolate->ThrowException(
      v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, msg)));
    // any VM calls after this point and before returning
    // to the javascript invoker will fail
}

double napi_get_number_from_value(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->NumberValue();
}

int napi_get_string_from_value(napi_env e, napi_value v,
                                char* buf, const int buf_size) {
    int len = napi_get_string_utf8_length(e, v);
    int copied = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
      ->WriteUtf8(
        buf,
        buf_size,
        0,
        v8::String::REPLACE_INVALID_UTF8 | v8::String::PRESERVE_ONE_BYTE_NULL);
    // add one for null ending
    return len - copied + 1;
}

int32_t napi_get_value_int32(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->Int32Value();
}

uint32_t napi_get_value_uint32(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->Uint32Value();
}

int64_t napi_get_value_int64(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->IntegerValue();
}

bool napi_get_value_bool(napi_env e, napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v)->BooleanValue();
}

int napi_get_string_length(napi_env e, napi_value v) {
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Length();
}

int napi_get_string_utf8_length(napi_env e, napi_value v) {
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Utf8Length();
}

int napi_get_string_utf8(napi_env e, napi_value v, char* buf, int bufsize) {
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
    ->WriteUtf8(
      buf,
      bufsize,
      0,
      v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8);
}

napi_value napi_coerce_to_object(napi_env e, napi_value v) {
  return v8impl::JsValueFromV8LocalValue(
    v8impl::V8LocalValueFromJsValue(v)->ToObject(
      v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_coerce_to_string(napi_env e, napi_value v) {
  return v8impl::JsValueFromV8LocalValue(
    v8impl::V8LocalValueFromJsValue(v)->ToString(
      v8impl::V8IsolateFromJsEnv(e)));
}


void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
               napi_destruct* destructor, napi_persistent* handle) {
  // object wrap api needs more thought
  // e.g. who deletes this object?
  v8impl::ObjectWrapWrapper* wrap =
      new v8impl::ObjectWrapWrapper(jsObject, nativeObj, destructor);
  if (handle != nullptr) {
    *handle = napi_create_persistent(
                  e,
                  v8impl::JsValueFromV8LocalValue(wrap->handle()));
  }
};

void* napi_unwrap(napi_env e, napi_value jsObject) {
  return v8impl::ObjectWrapWrapper::Unwrap(jsObject);
};

napi_persistent napi_create_persistent(napi_env e, napi_value v) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      new v8::Persistent<v8::Value>(
          isolate, v8impl::V8LocalValueFromJsValue(v));
  return v8impl::JsPersistentFromV8PersistentValue(thePersistent);
}

void napi_release_persistent(napi_env e, napi_persistent p) {
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  thePersistent->Reset();
  delete thePersistent;
}

napi_value napi_get_persistent_value(napi_env e, napi_persistent p) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  v8::Local<v8::Value> napi_value =
      v8::Local<v8::Value>::New(isolate, *thePersistent);
  return v8impl::JsValueFromV8LocalValue(napi_value);
};

napi_handle_scope napi_open_handle_scope(napi_env e) {
  return v8impl::JsHandleScopeFromV8HandleScope(
    new v8impl::HandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
}

void napi_close_handle_scope(napi_env e, napi_handle_scope scope) {
  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
}

napi_escapable_handle_scope napi_open_escapable_handle_scope(napi_env e) {
  return v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
    new v8impl::EscapableHandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
}

void napi_close_escapable_handle_scope(napi_env e,
                                       napi_escapable_handle_scope scope) {
  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
}

napi_value napi_escape_handle(napi_env e, napi_escapable_handle_scope scope,
                              napi_value escapee) {
  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  return v8impl::JsValueFromV8LocalValue(
    s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
}

napi_value napi_new_instance(napi_env e, napi_value cons,
                             int argc, napi_value *argv) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Function> v8cons = v8impl::V8LocalFunctionFromJsValue(cons);
  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Handle<v8::Value> result = v8cons->NewInstance(
                                     isolate->GetCurrentContext(),
                                     argc,
                                     args.data()).ToLocalChecked();
  return v8impl::JsValueFromV8LocalValue(result);
};

napi_value napi_make_callback(napi_env e, napi_value recv,
                              napi_value func, int argc, napi_value* argv) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> v8recv =
      v8impl::V8LocalValueFromJsValue(recv).As<v8::Object>();
  v8::Local<v8::Function> v8func =
      v8impl::V8LocalValueFromJsValue(func).As<v8::Function>();
  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Handle<v8::Value> result =
      node::MakeCallback(isolate, v8recv, v8func, argc, args.data());
  return v8impl::JsValueFromV8LocalValue(result);
}

bool napi_try_catch(napi_env e, napi_try_callback cbtry,
                    napi_catch_callback cbcatch, void* data) {
  v8::TryCatch try_catch;
  cbtry(e, data);
  if (try_catch.HasCaught()) {
    cbcatch;
    return true;
  }
  return false;
}

napi_value napi_buffer_copy(napi_env e, const char* data, uint32_t size) {
  return v8impl::JsValueFromV8LocalValue(
    node::Buffer::Copy(
        v8impl::V8IsolateFromJsEnv(e), data, size).ToLocalChecked());
}

bool napi_buffer_has_instance(napi_env e, napi_value v) {
  return node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(v));
}

char* napi_buffer_data(napi_env e, napi_value v) {
  return node::Buffer::Data(
      v8impl::V8LocalValueFromJsValue(v).As<v8::Object>());
}

size_t napi_buffer_length(napi_env e, napi_value v) {
  return node::Buffer::Length(v8impl::V8LocalValueFromJsValue(v));
}


///////////////////////////////////////////
// WILL GO AWAY
///////////////////////////////////////////
v8::Local<v8::Value> V8LocalValue(napi_value v) {
    return v8impl::V8LocalValueFromJsValue(v);
}

napi_value JsValue(v8::Local<v8::Value> v) {
    return v8impl::JsValueFromV8LocalValue(v);
}

v8::Persistent<v8::Value>* V8PersistentValue(napi_persistent p) {
    return v8impl::V8PersistentValueFromJsPersistentValue(p);
}
