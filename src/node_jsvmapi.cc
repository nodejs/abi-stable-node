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

#include "node_jsvmapi_internal.h"
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <vector>
#include <string.h>

void napi_module_register(void* mod) {
  node::node_module_register(mod);
}

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

  // In node v0.10 version of v8, there is no EscapableHandleScope and the
  // node v0.10 port use HandleScope::Close(Local<T> v) to mimic the behavior
  // of a EscapableHandleScope::Escape(Local<T> v), but it is not the same
  // semantics. This is an example of where the api abstraction fail to work
  // across different versions.
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
  }

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
  }

  static v8::Persistent<v8::Value>* V8PersistentValueFromJsPersistentValue(
                                                        napi_persistent per) {
    return (v8::Persistent<v8::Value>*) per;
  }

  static napi_weakref JsWeakRefFromV8PersistentValue(
                                             v8::Persistent<v8::Value> *per) {
    return (napi_weakref) per;
  }

  static v8::Persistent<v8::Value>* V8PersistentValueFromJsWeakRefValue(
                                                           napi_weakref per) {
    return (v8::Persistent<v8::Value>*) per;
  }

  static void WeakRefCallback(const v8::WeakCallbackInfo<int>& data) {
  }

  class TryCatch: public v8::TryCatch {
    public:
      explicit TryCatch(v8::Isolate *isolate):
        v8::TryCatch(isolate), _isolate(isolate) {}

      ~TryCatch() {
        if (HasCaught()) {
          lastException().Reset(_isolate, Exception());
        }
      }

      static v8::Persistent<v8::Value> & lastException() {
        static v8::Persistent<v8::Value> theException;
        return theException;
      }
    private:
      v8::Isolate *_isolate;
  };

//=== Function napi_callback wrapper =================================

  static const int kDataIndex = 0;

  static const int kFunctionIndex = 1;
  static const int kFunctionFieldCount = 2;

  static const int kGetterIndex = 1;
  static const int kSetterIndex = 2;
  static const int kAccessorFieldCount = 3;

  // Interface implemented by classes that wrap V8 function and property
  // callback info.
  interface CallbackWrapper {
    virtual napi_value This() = 0;
    virtual napi_value Holder() = 0;
    virtual bool IsConstructCall() = 0;
    virtual void* Data() = 0;
    virtual int ArgsLength() = 0;
    virtual void Args(napi_value* buffer, size_t bufferlength) = 0;
    virtual void SetReturnValue(napi_value v) = 0;
  };

  template <typename T, int I>
  class CallbackWrapperBase : public CallbackWrapper {
    public:
      CallbackWrapperBase(const T& cbinfo)
        : _cbinfo(cbinfo),
          _cbdata(cbinfo.Data().As<v8::Object>()) {
      }

      virtual napi_value This() override {
        return JsValueFromV8LocalValue(_cbinfo.This());
      }

      virtual napi_value Holder() override {
        return JsValueFromV8LocalValue(_cbinfo.Holder());
      }

      virtual bool IsConstructCall() override {
        return false;
      }

      virtual void* Data() override {
        return _cbdata->GetInternalField(kDataIndex).As<v8::External>()->Value();
      }

    protected:
      void InvokeCallback() {
        napi_callback_info cbinfo_wrapper = reinterpret_cast<napi_callback_info>(
          static_cast<CallbackWrapper*>(this));
        napi_callback cb = reinterpret_cast<napi_callback>(
          _cbdata->GetInternalField(I).As<v8::External>()->Value());
        v8::Isolate* isolate = _cbinfo.GetIsolate();
        cb(v8impl::JsEnvFromV8Isolate(isolate), cbinfo_wrapper);

        if (!TryCatch::lastException().IsEmpty()) {
          isolate->ThrowException(TryCatch::lastException().Get(isolate));
          TryCatch::lastException().Reset();
        }
      }

      const T& _cbinfo;
      const v8::Local<v8::Object> _cbdata;
  };

  class FunctionCallbackWrapper : public CallbackWrapperBase<
      v8::FunctionCallbackInfo<v8::Value>, kFunctionIndex> {
    public:
      static void Invoke(const v8::FunctionCallbackInfo<v8::Value> &info) {
        FunctionCallbackWrapper cbwrapper(info);
        cbwrapper.InvokeCallback();
      }

      FunctionCallbackWrapper(
        const v8::FunctionCallbackInfo<v8::Value>& cbinfo)
        : CallbackWrapperBase(cbinfo) {
      }

      virtual bool IsConstructCall() override {
        return _cbinfo.IsConstructCall();
      }

      virtual int ArgsLength() override {
        return _cbinfo.Length();
      }

      virtual void Args(napi_value* buffer, size_t bufferlength) override {
        int i = 0;
        // size_t appropriate for the buffer length parameter?
        // Probably this API is not the way to go.
        int min =
          static_cast<int>(bufferlength) < _cbinfo.Length() ?
          static_cast<int>(bufferlength) : _cbinfo.Length();

        for (; i < min; i += 1) {
          buffer[i] = v8impl::JsValueFromV8LocalValue(_cbinfo[i]);
        }

        if (i < static_cast<int>(bufferlength)) {
          napi_value undefined = v8impl::JsValueFromV8LocalValue(
            v8::Undefined(v8::Isolate::GetCurrent()));
          for (; i < static_cast<int>(bufferlength); i += 1) {
            buffer[i] = undefined;
          }
        }
      }

      virtual void SetReturnValue(napi_value v) override {
        v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
        _cbinfo.GetReturnValue().Set(val);
      }
  };

  class GetterCallbackWrapper : public CallbackWrapperBase<
      v8::PropertyCallbackInfo<v8::Value>, kGetterIndex> {
    public:
      static void Invoke(
          v8::Local<v8::Name> property,
          const v8::PropertyCallbackInfo<v8::Value>& info) {
        GetterCallbackWrapper cbwrapper(info);
        cbwrapper.InvokeCallback();
      }

      GetterCallbackWrapper(
        const v8::PropertyCallbackInfo<v8::Value>& cbinfo)
        : CallbackWrapperBase(cbinfo) {
      }

      virtual int ArgsLength() override {
        return 0;
      }

      virtual void Args(napi_value* buffer, size_t bufferlength) override {
        if (bufferlength > 0) {
          napi_value undefined = v8impl::JsValueFromV8LocalValue(
            v8::Undefined(v8::Isolate::GetCurrent()));
          for (int i = 0; i < static_cast<int>(bufferlength); i += 1) {
            buffer[i] = undefined;
          }
        }
      }

      virtual void SetReturnValue(napi_value v) override {
        v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
        _cbinfo.GetReturnValue().Set(val);
      }
  };

  class SetterCallbackWrapper : public CallbackWrapperBase<
      v8::PropertyCallbackInfo<void>, kSetterIndex> {
    public:
      static void Invoke(
          v8::Local<v8::Name> property,
          v8::Local<v8::Value> v,
          const v8::PropertyCallbackInfo<void>& info) {
        SetterCallbackWrapper cbwrapper(info, v);
        cbwrapper.InvokeCallback();
      }

      SetterCallbackWrapper(
        const v8::PropertyCallbackInfo<void>& cbinfo,
        const v8::Local<v8::Value>& value)
        : CallbackWrapperBase(cbinfo),
          _value(value) {
      }

      virtual int ArgsLength() override {
        return 1;
      }

      virtual void Args(napi_value* buffer, size_t bufferlength) override {
        if (bufferlength > 0) {
          buffer[0] = v8impl::JsValueFromV8LocalValue(_value);

          if (bufferlength > 1) {
            napi_value undefined = v8impl::JsValueFromV8LocalValue(
              v8::Undefined(v8::Isolate::GetCurrent()));
            for (int i = 1; i < static_cast<int>(bufferlength); i += 1) {
              buffer[i] = undefined;
            }
          }
        }
      }

      virtual void SetReturnValue(napi_value v) override {
        // Cannot set the return value of a setter.
        assert(false);
      }

    private:
      const v8::Local<v8::Value>& _value;
  };

  class ObjectWrapWrapper: public node::ObjectWrap  {
    public:
      ObjectWrapWrapper(napi_value jsObject, void* nativeObj,
                        napi_destruct destructor) {
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
      napi_destruct _destructor;
      void* _nativeObj;
  };

  // Creates an object to be made available to the static function callback
  // wrapper, used to retrieve the native callback function and data pointer.
  v8::Local<v8::Object> CreateFunctionCallbackData(
      napi_env e,
      napi_callback cb,
      void* data) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    v8::Local<v8::ObjectTemplate> otpl = v8::ObjectTemplate::New(isolate);
    otpl->SetInternalFieldCount(v8impl::kFunctionFieldCount);
    v8::Local<v8::Object> cbdata = otpl->NewInstance(context).ToLocalChecked();

    cbdata->SetInternalField(
      v8impl::kFunctionIndex,
      v8::External::New(isolate, reinterpret_cast<void*>(cb)));

    if (data) {
      cbdata->SetInternalField(
        v8impl::kDataIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(data)));
    }

    return cbdata;
  }

  // Creates an object to be made available to the static getter/setter
  // callback wrapper, used to retrieve the native getter/setter callback
  // function and data pointer.
  v8::Local<v8::Object> CreateAccessorCallbackData(
      napi_env e,
      napi_callback getter,
      napi_callback setter,
      void* data) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    v8::Local<v8::ObjectTemplate> otpl = v8::ObjectTemplate::New(isolate);
    otpl->SetInternalFieldCount(v8impl::kAccessorFieldCount);
    v8::Local<v8::Object> cbdata = otpl->NewInstance(context).ToLocalChecked();

    if (getter) {
      cbdata->SetInternalField(
        v8impl::kGetterIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(getter)));
    }

    if (setter) {
      cbdata->SetInternalField(
        v8impl::kSetterIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(setter)));
    }

    if (data) {
      cbdata->SetInternalField(
        v8impl::kDataIndex,
        v8::External::New(isolate, reinterpret_cast<void*>(data)));
    }

    return cbdata;
  }

}  // end of namespace v8impl

// We define a macro here because, with full error handling, we may want to
// perform additional actions at the top of our public APIs, such as bail,
// returning an error code at the very top of each public API in the case that
// there's already an exception pending in v8impl::TryCatch::lastException
#define NAPI_PREAMBLE(e) \
  v8impl::TryCatch tryCatch(v8impl::V8IsolateFromJsEnv((e)))

napi_env napi_get_current_env() {
  return v8impl::JsEnvFromV8Isolate(v8::Isolate::GetCurrent());
}

napi_value napi_create_function(
    napi_env e,
    napi_callback cb,
    void* data) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);

  v8::Local<v8::Object> cbdata =
    v8impl::CreateFunctionCallbackData(e, cb, data);
  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
    isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  retval = scope.Escape(tpl->GetFunction());
  return v8impl::JsValueFromV8LocalValue(retval);
}

napi_value napi_create_constructor(
    napi_env e,
    char* utf8name,
    napi_callback cb,
    void* data,
    int property_count,
    napi_property_descriptor* properties) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> cbdata =
    v8impl::CreateFunctionCallbackData(e, cb, data);
  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
    isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::String> namestring =
      v8::String::NewFromUtf8(isolate, utf8name,
          v8::NewStringType::kInternalized).ToLocalChecked();
  tpl->SetClassName(namestring);

  for (int i = 0; i < property_count; i++) {
    napi_property_descriptor* p = properties + i;
    v8::Local<v8::String> propertyname =
      v8::String::NewFromUtf8(isolate, p->utf8name,
        v8::NewStringType::kInternalized).ToLocalChecked();

    v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(p->attributes);

    // This code is similar to that in napi_define_property(); the
    // difference is it applies to a template instead of an object.
    if (p->method) {
      v8::Local<v8::Object> cbdata = v8impl::CreateFunctionCallbackData(
        e, p->method, p->data);

      v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(
        isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata,
        v8::Signature::New(isolate, tpl));
      t->SetClassName(propertyname);

      tpl->PrototypeTemplate()->Set(propertyname, t, attributes);
    }
    else if (p->getter || p->setter) {
      v8::Local<v8::Object> cbdata = v8impl::CreateAccessorCallbackData(
        e, p->getter, p->setter, p->data);

      tpl->PrototypeTemplate()->SetAccessor(
        propertyname,
        v8impl::GetterCallbackWrapper::Invoke,
        p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
        cbdata,
        v8::AccessControl::DEFAULT,
        attributes);
    }
    else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);
      tpl->PrototypeTemplate()->Set(
        propertyname,
        value,
        attributes);
    }
  }

  retval = scope.Escape(tpl->GetFunction());
  return v8impl::JsValueFromV8LocalValue(retval);
}

void napi_set_function_name(napi_env e, napi_value func,
                            napi_propertyname name) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
  v8func->SetName(
      v8impl::V8LocalValueFromJsPropertyName(name).As<v8::String>());
}

void napi_set_return_value(napi_env e,
                           napi_callback_info cbinfo, napi_value v) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  info->SetReturnValue(v);
}

napi_propertyname napi_property_name(napi_env e, const char* utf8name) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::String> namestring =
      v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), utf8name,
          v8::NewStringType::kInternalized).ToLocalChecked();
  return reinterpret_cast<napi_propertyname>(
      v8impl::JsValueFromV8LocalValue(namestring));
}

napi_value napi_get_propertynames(napi_env e, napi_value o) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Array> array = obj->GetPropertyNames();
  return v8impl::JsValueFromV8LocalValue(array);
}

void napi_set_property(napi_env e, napi_value o,
                       napi_propertyname k, napi_value v) {
  NAPI_PREAMBLE(e);
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
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);

  return obj->Has(key);
}

napi_value napi_get_property(napi_env e, napi_value o, napi_propertyname k) {
  NAPI_PREAMBLE(e);
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
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

  obj->Set(i, val);

  // This implementation is missing a lot of details, notably error
  // handling on invalid inputs and regarding what happens in the
  // Set operation (error thrown, key is invalid, the bool return
  // value of Set)
}

bool napi_has_element(napi_env e, napi_value o, uint32_t i) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();

  return obj->Has(i);
}

napi_value napi_get_element(napi_env e, napi_value o, uint32_t i) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Value> val = obj->Get(i);
  // This implementation is missing a lot of details, notably error
  // handling on invalid inputs and regarding what happens in the
  // Set operation (error thrown, key is invalid, the bool return
  // value of Set)
  return v8impl::JsValueFromV8LocalValue(val);
}

void napi_define_property(napi_env e, napi_value o,
    napi_property_descriptor* p) {
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Name> name =
    v8::String::NewFromUtf8(isolate, p->utf8name,
      v8::NewStringType::kInternalized).ToLocalChecked();

  v8::PropertyAttribute attributes =
    static_cast<v8::PropertyAttribute>(p->attributes);

  if (p->method) {
    v8::Local<v8::Object> cbdata = v8impl::CreateFunctionCallbackData(
      e, p->method, p->data);

    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

    obj->DefineOwnProperty(
      context,
      name,
      t->GetFunction(),
      attributes);
  }
  else if (p->getter || p->setter) {
    v8::Local<v8::Object> cbdata = v8impl::CreateAccessorCallbackData(
      e, p->getter, p->setter, p->data);

    obj->SetAccessor(
      context,
      name,
      v8impl::GetterCallbackWrapper::Invoke,
      p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
      cbdata,
      v8::AccessControl::DEFAULT,
      attributes);
  }
  else {
    v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);
    obj->DefineOwnProperty(
      context,
      name,
      value,
      attributes);
  }
}

bool napi_is_array(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
  return val->IsArray();
}

uint32_t napi_get_array_length(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Array> arr =
      v8impl::V8LocalValueFromJsValue(v).As<v8::Array>();
  return arr->Length();
}

bool napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Value> a = v8impl::V8LocalValueFromJsValue(lhs);
  v8::Local<v8::Value> b = v8impl::V8LocalValueFromJsValue(rhs);
  return a->StrictEquals(b);
}

napi_value napi_get_prototype(napi_env e, napi_value o) {
  NAPI_PREAMBLE(e);
  v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
  v8::Local<v8::Value> val = obj->GetPrototype();
  return v8impl::JsValueFromV8LocalValue(val);
}

napi_value napi_create_object(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Object::New(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_create_array(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Array::New(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_create_array_with_length(napi_env e, int length) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Array::New(v8impl::V8IsolateFromJsEnv(e), length));
}

napi_value napi_create_string(napi_env e, const char* s) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s));
}

napi_value napi_create_string_with_length(napi_env e,
                                          const char* s, size_t length) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s,
             v8::NewStringType::kNormal,
             static_cast<int>(length)).ToLocalChecked());
}

napi_value napi_create_number(napi_env e, double v) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));
}

napi_value napi_create_boolean(napi_env e, bool b) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Boolean::New(v8impl::V8IsolateFromJsEnv(e), b));
}

napi_value napi_create_symbol(napi_env e, const char* s) {
  NAPI_PREAMBLE(e);
  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  if (s == NULL) {
    return v8impl::JsValueFromV8LocalValue(v8::Symbol::New(isolate));
  } else {
    v8::Local<v8::String> string = v8::String::NewFromUtf8(isolate, s);
    return v8impl::JsValueFromV8LocalValue(
               v8::Symbol::New(isolate, string));
  }
}

napi_value napi_create_error(napi_env e, napi_value msg) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      v8::Exception::Error(
          v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

napi_value napi_create_type_error(napi_env e, napi_value msg) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      v8::Exception::TypeError(
          v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

napi_valuetype napi_get_type_of_value(napi_env e, napi_value vv) {
  v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(vv);
  NAPI_PREAMBLE(e);

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
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Undefined(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_null(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::Null(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_false(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::False(v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_get_true(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
             v8::True(v8impl::V8IsolateFromJsEnv(e)));
}

int napi_get_cb_args_length(napi_env e, napi_callback_info cbinfo) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  return info->ArgsLength();
}

bool napi_is_construct_call(napi_env e, napi_callback_info cbinfo) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  return info->IsConstructCall();
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
void napi_get_cb_args(napi_env e, napi_callback_info cbinfo,
                      napi_value* buffer, size_t bufferlength) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  info->Args(buffer, bufferlength);
}

napi_value napi_get_cb_this(napi_env e, napi_callback_info cbinfo) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  return info->This();
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_value napi_get_cb_holder(napi_env e, napi_callback_info cbinfo) {
  NAPI_PREAMBLE(e);
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  return info->Holder();
}

void* napi_get_cb_data(napi_env e, napi_callback_info cbinfo) {
  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);
  return info->Data();
}

napi_value napi_call_function(napi_env e, napi_value recv,
                              napi_value func, int argc, napi_value* argv) {
  NAPI_PREAMBLE(e);
  std::vector<v8::Handle<v8::Value>> args(argc);

  v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
  v8::Handle<v8::Object> v8recv =
      v8impl::V8LocalValueFromJsValue(recv)->ToObject();
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }
  v8::Handle<v8::Value> result = v8func->Call(v8recv, argc, args.data());
  return v8impl::JsValueFromV8LocalValue(result);
}

napi_value napi_get_global(napi_env e) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  // TODO(ianhall): what if we need the global object from a different
  // context in the same isolate?
  // Should napi_env be the current context rather than the current isolate?
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return v8impl::JsValueFromV8LocalValue(context->Global());
}

void napi_throw(napi_env e, napi_value error) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

  isolate->ThrowException(
      v8impl::V8LocalValueFromJsValue(error));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
}

void napi_throw_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

  isolate->ThrowException(
      v8::Exception::Error(v8::String::NewFromUtf8(isolate, msg)));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
}

void napi_throw_type_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

  isolate->ThrowException(
      v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, msg)));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
}

double napi_get_number_from_value(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v)->NumberValue();
}

int napi_get_string_from_value(napi_env e, napi_value v,
                               char* buf, const int buf_size) {
  NAPI_PREAMBLE(e);
  if (napi_get_type_of_value(e, v) == napi_number) {
    v8::String::Utf8Value str(v8impl::V8LocalValueFromJsValue(v));
    int len = str.length();
    if (buf_size > len) {
      memcpy(buf, *str, len);
      return 0;
    } else {
      memcpy(buf, *str, buf_size - 1);
      return len - buf_size + 1;
    }
  } else {
    int len = napi_get_string_utf8_length(e, v);
    int copied = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
        ->WriteUtf8(
            buf,
            buf_size,
            0,
            v8::String::REPLACE_INVALID_UTF8 |
            v8::String::PRESERVE_ONE_BYTE_NULL);
     // add one for null ending
     return len - copied + 1;
  }
}

int32_t napi_get_value_int32(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v)->Int32Value();
}

uint32_t napi_get_value_uint32(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v)->Uint32Value();
}

int64_t napi_get_value_int64(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v)->IntegerValue();
}

bool napi_get_value_bool(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v)->BooleanValue();
}

int napi_get_string_length(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Length();
}

int napi_get_string_utf8_length(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Utf8Length();
}

int napi_get_string_utf8(napi_env e, napi_value v, char* buf, int bufsize) {
  NAPI_PREAMBLE(e);
  return v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
      ->WriteUtf8(
          buf,
          bufsize,
          0,
          v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8);
}

napi_value napi_coerce_to_object(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      v8impl::V8LocalValueFromJsValue(v)->ToObject(
      v8impl::V8IsolateFromJsEnv(e)));
}

napi_value napi_coerce_to_string(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      v8impl::V8LocalValueFromJsValue(v)->ToString(
          v8impl::V8IsolateFromJsEnv(e)));
}

void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
               napi_destruct destructor, napi_weakref* handle) {
  NAPI_PREAMBLE(e);
  // object wrap api needs more thought
  // e.g. who deletes this object?
  v8impl::ObjectWrapWrapper* wrap =
      new v8impl::ObjectWrapWrapper(jsObject, nativeObj, destructor);
  if (handle != nullptr) {
    *handle = napi_create_weakref(
                  e,
                  v8impl::JsValueFromV8LocalValue(wrap->handle()));
  }
}

void* napi_unwrap(napi_env e, napi_value jsObject) {
  NAPI_PREAMBLE(e);
  return v8impl::ObjectWrapWrapper::Unwrap(jsObject);
}

napi_persistent napi_create_persistent(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      new v8::Persistent<v8::Value>(
          isolate, v8impl::V8LocalValueFromJsValue(v));
  return v8impl::JsPersistentFromV8PersistentValue(thePersistent);
}

void napi_release_persistent(napi_env e, napi_persistent p) {
  NAPI_PREAMBLE(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  thePersistent->Reset();
  delete thePersistent;
}

napi_value napi_get_persistent_value(napi_env e, napi_persistent p) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  v8::Local<v8::Value> napi_value =
      v8::Local<v8::Value>::New(isolate, *thePersistent);
  return v8impl::JsValueFromV8LocalValue(napi_value);
}

napi_weakref napi_create_weakref(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      new v8::Persistent<v8::Value>(
          isolate, v8impl::V8LocalValueFromJsValue(v));
  thePersistent->SetWeak(static_cast<int*>(nullptr), v8impl::WeakRefCallback,
                         v8::WeakCallbackType::kParameter);
  // need to mark independent?
  return v8impl::JsWeakRefFromV8PersistentValue(thePersistent);
}

bool napi_get_weakref_value(napi_env e, napi_weakref w, napi_value* pv) {
  NAPI_PREAMBLE(e);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsWeakRefValue(w);
  v8::Local<v8::Value> v =
      v8::Local<v8::Value>::New(isolate, *thePersistent);
  if (v.IsEmpty()) {
    *pv = nullptr;
    return false;
  }
  *pv = v8impl::JsValueFromV8LocalValue(v);
  return true;
}

void napi_release_weakref(napi_env e, napi_weakref w) {
  NAPI_PREAMBLE(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsWeakRefValue(w);
  thePersistent->Reset();
  delete thePersistent;
}

napi_handle_scope napi_open_handle_scope(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsHandleScopeFromV8HandleScope(
      new v8impl::HandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
}

void napi_close_handle_scope(napi_env e, napi_handle_scope scope) {
  NAPI_PREAMBLE(e);
  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
}

napi_escapable_handle_scope napi_open_escapable_handle_scope(napi_env e) {
  NAPI_PREAMBLE(e);
  return v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
      new v8impl::EscapableHandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
}

void napi_close_escapable_handle_scope(napi_env e,
                                       napi_escapable_handle_scope scope) {
  NAPI_PREAMBLE(e);
  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
}

napi_value napi_escape_handle(napi_env e, napi_escapable_handle_scope scope,
                              napi_value escapee) {
  NAPI_PREAMBLE(e);
  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  return v8impl::JsValueFromV8LocalValue(
      s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
}

napi_value napi_new_instance(napi_env e, napi_value cons,
                             int argc, napi_value *argv) {
  NAPI_PREAMBLE(e);
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
}

bool napi_instanceof(napi_env e, napi_value obj, napi_value cons) {
  bool returnValue = false;

  v8::Local<v8::Value> v8Obj = v8impl::V8LocalValueFromJsValue(obj);
  v8::Local<v8::Value> v8Cons = v8impl::V8LocalValueFromJsValue(cons);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

  if (!v8Cons->IsFunction()) {
    napi_throw_type_error(e, "constructor must be a function");

    // Error handling needs to be done here
    return false;
  }

  v8Cons =
    v8Cons->ToObject()->Get(v8::String::NewFromUtf8(isolate, "prototype"));

  if (!v8Cons->IsObject()) {
    napi_throw_type_error(e, "constructor prototype must be an object");

    // Error handling needs to be done here
    return false;
  }

  if (!v8Obj->StrictEquals(v8Cons)) {
    for (v8::Local<v8::Value> originalObj = v8Obj;
        !(v8Obj->IsNull() || v8Obj->IsUndefined());
        v8Obj = v8Obj->ToObject()->GetPrototype()) {
      if (v8Obj->StrictEquals(v8Cons)) {
        returnValue =
          !(originalObj->IsNumber() ||
            originalObj->IsBoolean() ||
            originalObj->IsString());
        break;
      }
    }
  }

  return returnValue;
}

napi_value napi_make_external(napi_env e, napi_value v) {

  // v8impl::TryCatch doesn't make sense here since we're not calling into the
  // engine at all.
  return v;
}

napi_value napi_make_callback(napi_env e, napi_value recv,
                              napi_value func, int argc, napi_value* argv) {
  NAPI_PREAMBLE(e);
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

// Methods to support catching exceptions
bool napi_is_exception_pending(napi_env) {
  return !v8impl::TryCatch::lastException().IsEmpty();
}

napi_value napi_get_and_clear_last_exception(napi_env e) {

  // TODO: Is there a chance that an exception will be thrown in the process of
  // attempting to retrieve the global static exception?
  napi_value returnValue;
  if (v8impl::TryCatch::lastException().IsEmpty()) {
    returnValue = napi_get_undefined_(e);
  } else {
    returnValue = v8impl::JsValueFromV8LocalValue(
      v8impl::TryCatch::lastException().Get(v8impl::V8IsolateFromJsEnv(e)));
    v8impl::TryCatch::lastException().Reset();
  }
  return returnValue;
}

napi_value napi_buffer_new(napi_env e, char* data, uint32_t size) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      node::Buffer::New(
          v8impl::V8IsolateFromJsEnv(e), data, size).ToLocalChecked());
}

napi_value napi_buffer_copy(napi_env e, const char* data, uint32_t size) {
  NAPI_PREAMBLE(e);
  return v8impl::JsValueFromV8LocalValue(
      node::Buffer::Copy(
          v8impl::V8IsolateFromJsEnv(e), data, size).ToLocalChecked());
}

bool napi_buffer_has_instance(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(v));
}

char* napi_buffer_data(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return node::Buffer::Data(
      v8impl::V8LocalValueFromJsValue(v).As<v8::Object>());
}

size_t napi_buffer_length(napi_env e, napi_value v) {
  NAPI_PREAMBLE(e);
  return node::Buffer::Length(v8impl::V8LocalValueFromJsValue(v));
}
