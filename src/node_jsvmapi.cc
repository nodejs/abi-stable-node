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
  struct CallbackWrapper {
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

  class ObjectWrapWrapper: public node::ObjectWrap {
    public:
      ObjectWrapWrapper(v8::Local<v8::Object> jsObject, void* nativeObj,
                        napi_destruct destructor) {
        _destructor = destructor;
        _nativeObj = nativeObj;
        Wrap(jsObject);
      }

      void* getValue() {
        return _nativeObj;
      }

      static void* Unwrap(v8::Local<v8::Object> jsObject) {
        ObjectWrapWrapper* wrapper =
            ObjectWrap::Unwrap<ObjectWrapWrapper>(jsObject);
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

#define RETURN_STATUS_IF_FALSE(condition, status)                       \
  do {                                                                  \
    if (!(condition)) {                                                 \
      return napi_set_last_error((status));                             \
    }                                                                   \
  } while(0)

#define CHECK_ARG(arg)                                                  \
  RETURN_STATUS_IF_FALSE((arg), napi_invalid_arg)

#define CHECK_MAYBE_EMPTY(maybe, status)                                \
  RETURN_STATUS_IF_FALSE(!((maybe).IsEmpty()), (status))

#define CHECK_MAYBE_NOTHING(maybe, status)                              \
  RETURN_STATUS_IF_FALSE(!((maybe).IsNothing()), (status))

#define NAPI_PREAMBLE(e)                                                \
  do {                                                                  \
    CHECK_ARG(e);                                                       \
    RETURN_STATUS_IF_FALSE(v8impl::TryCatch::lastException().IsEmpty(), \
      napi_pending_exception);                                          \
    napi_clear_last_error();                                            \
    v8impl::TryCatch tryCatch(v8impl::V8IsolateFromJsEnv((e)));         \
  } while(0)

#define CHECK_TO_TYPE(type, context, result, src, status)               \
  do {                                                                  \
    auto maybe =                                                        \
      v8impl::V8LocalValueFromJsValue((src))->To##type((context));      \
    CHECK_MAYBE_EMPTY(maybe, (status));                                 \
    result = maybe.ToLocalChecked();                                    \
  } while(0)

#define CHECK_TO_OBJECT(context, result, src)                           \
    CHECK_TO_TYPE(Object, context, result, src, napi_object_expected)

#define CHECK_TO_STRING(context, result, src)                           \
    CHECK_TO_TYPE(String, context, result, src, napi_string_expected)

#define CHECK_NEW_FROM_UTF8_LEN(isolate, result, str, len)              \
  do {                                                                  \
    auto name_maybe = v8::String::NewFromUtf8((isolate), (str),         \
      v8::NewStringType::kInternalized, (len));                         \
    CHECK_MAYBE_EMPTY(name_maybe, napi_generic_failure);                \
    result = name_maybe.ToLocalChecked();                               \
  } while(0)

#define CHECK_NEW_FROM_UTF8(isolate, result, str)                       \
    CHECK_NEW_FROM_UTF8_LEN((isolate), (result), (str), -1)

// Static last error returned from napi_get_last_error_info
napi_extended_error_info static_last_error;

// Warning: Keep in-sync with napi_status enum
const char* error_messages[] = {
  nullptr,
  "Invalid pointer passed as argument",
  "An object was expected",
  "A string was expected",
  "A function was expected",
  "Unknown failure",
  "An exception is pending"
};

void napi_clear_last_error() {
  static_last_error.error_code = napi_ok;

  // TODO: Should this be a callback?
  static_last_error.engine_error_code = 0;
  static_last_error.engine_reserved = nullptr;
}

const napi_extended_error_info* napi_get_last_error_info() {
  static_assert(sizeof(error_messages) / sizeof(*error_messages) == napi_status_last, 
    "Count of error messages must match count of error values");
  assert(static_last_error.error_code < napi_status_last);

  // Wait until someone requests the last error information to fetch the error message string
  static_last_error.error_message = error_messages[static_last_error.error_code];

  return &static_last_error;
}

napi_status napi_set_last_error(napi_status error_code,
                                uint32_t engine_error_code = 0,
                                void* engine_reserved = nullptr) {
  static_last_error.error_code = error_code;
  static_last_error.engine_error_code = engine_error_code;
  static_last_error.engine_reserved = engine_reserved;

  return error_code;
}

napi_status napi_get_current_env(napi_env* e) {
  CHECK_ARG(e);

  *e = v8impl::JsEnvFromV8Isolate(v8::Isolate::GetCurrent());
  return napi_ok;
}

napi_status napi_create_function(
    napi_env e,
    napi_callback cb,
    void* data,
    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);

  v8::Local<v8::Object> cbdata =
    v8impl::CreateFunctionCallbackData(e, cb, data);

  RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
    isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  retval = scope.Escape(tpl->GetFunction());
  *result = v8impl::JsValueFromV8LocalValue(retval);

  return napi_ok;
}

napi_status napi_create_constructor(
    napi_env e,
    char* utf8name,
    napi_callback cb,
    void* data,
    int property_count,
    napi_property_descriptor* properties,
    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> retval;

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> cbdata =
    v8impl::CreateFunctionCallbackData(e, cb, data);

  RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
    isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::String> namestring;
  CHECK_NEW_FROM_UTF8(isolate, namestring, utf8name);
  tpl->SetClassName(namestring);

  for (int i = 0; i < property_count; i++) {
    napi_property_descriptor* p = properties + i;
    v8::Local<v8::String> propertyname;
    CHECK_NEW_FROM_UTF8(isolate, propertyname, p->utf8name);

    v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(p->attributes);

    // This code is similar to that in napi_define_property(); the
    // difference is it applies to a template instead of an object.
    if (p->method) {
      v8::Local<v8::Object> cbdata = v8impl::CreateFunctionCallbackData(
        e, p->method, p->data);

      RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

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
  *result = v8impl::JsValueFromV8LocalValue(retval);

  return napi_ok;
}

napi_status napi_set_function_name(napi_env e, napi_value func,
                                   napi_propertyname name) {
  NAPI_PREAMBLE(e);

  v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
  v8func->SetName(
      v8impl::V8LocalValueFromJsPropertyName(name).As<v8::String>());

  return napi_ok;
}

napi_status napi_set_return_value(napi_env e,
                                  napi_callback_info cbinfo, napi_value v) {
  NAPI_PREAMBLE(e);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  info->SetReturnValue(v);
  return napi_ok;
}

napi_status napi_property_name(napi_env e,
                               const char* utf8name,
                               napi_propertyname* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> name;
  CHECK_NEW_FROM_UTF8(isolate, name, utf8name);

  *result = reinterpret_cast<napi_propertyname>(
    v8impl::JsValueFromV8LocalValue(name));

  return napi_ok;
}

napi_status napi_get_propertynames(napi_env e, napi_value o, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, o);

  auto maybe_propertynames = obj->GetPropertyNames(context);

  CHECK_MAYBE_EMPTY(maybe_propertynames, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe_propertynames.ToLocalChecked());
  return napi_ok;
}

napi_status napi_set_property(napi_env e,
                                  napi_value o,
                                  napi_propertyname k,
                                  napi_value v) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);
  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

  v8::Maybe<bool> set_maybe = obj->Set(context, key, val);

  RETURN_STATUS_IF_FALSE(set_maybe.FromMaybe(false), napi_generic_failure);
  return napi_ok;
}

napi_status napi_has_property(napi_env e, napi_value o, napi_propertyname k, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);
  v8::Maybe<bool> has_maybe = obj->Has(context, key);

  CHECK_MAYBE_NOTHING(has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return napi_ok;
}

napi_status napi_get_property(napi_env e,
                                  napi_value o,
                                  napi_propertyname k,
                                  napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsPropertyName(k);
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  auto get_maybe = obj->Get(context, key);

  CHECK_MAYBE_EMPTY(get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return napi_ok;
}

napi_status napi_set_element(napi_env e, napi_value o, uint32_t i, napi_value v) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
  auto set_maybe = obj->Set(context, i, val);

  RETURN_STATUS_IF_FALSE(set_maybe.FromMaybe(false), napi_generic_failure);

  return napi_ok;
}

napi_status napi_has_element(napi_env e, napi_value o, uint32_t i, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  v8::Maybe<bool> has_maybe = obj->Has(context, i);

  CHECK_MAYBE_NOTHING(has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return napi_ok;
}

napi_status napi_get_element(napi_env e,
                             napi_value o,
                             uint32_t i,
                             napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, o);

  auto get_maybe = obj->Get(context, i);

  CHECK_MAYBE_EMPTY(get_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(get_maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_define_property(napi_env e, napi_value o,
                                 napi_property_descriptor* p) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, o);

  v8::Local<v8::Name> name;
  CHECK_NEW_FROM_UTF8(isolate, name, p->utf8name);

  v8::PropertyAttribute attributes =
    static_cast<v8::PropertyAttribute>(p->attributes);

  if (p->method) {
    v8::Local<v8::Object> cbdata = v8impl::CreateFunctionCallbackData(
      e, p->method, p->data);

    RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

    auto define_maybe = obj->DefineOwnProperty(
      context,
      name,
      t->GetFunction(),
      attributes);

    // IsNothing seems like a serious failure,
    // should we return a different error code if the define failed?
    if (define_maybe.IsNothing() || !define_maybe.FromMaybe(false))
    {
      return napi_set_last_error(napi_generic_failure);
    }
  }
  else if (p->getter || p->setter) {
    v8::Local<v8::Object> cbdata = v8impl::CreateAccessorCallbackData(
      e, p->getter, p->setter, p->data);

    auto set_maybe = obj->SetAccessor(
      context,
      name,
      v8impl::GetterCallbackWrapper::Invoke,
      p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
      cbdata,
      v8::AccessControl::DEFAULT,
      attributes);

    // IsNothing seems like a serious failure,
    // should we return a different error code if the set failed?
    if (set_maybe.IsNothing() || !set_maybe.FromMaybe(false))
    {
      return napi_set_last_error(napi_generic_failure);
    }
  }
  else {
    v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);

    auto define_maybe = obj->DefineOwnProperty(
      context,
      name,
      value,
      attributes);

    // IsNothing seems like a serious failure,
    // should we return a different error code if the define failed?
    if (define_maybe.IsNothing() || !define_maybe.FromMaybe(false))
    {
      return napi_set_last_error(napi_generic_failure);
    }
  }

  return napi_ok;
}

napi_status napi_is_array(napi_env e, napi_value v, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

  *result = val->IsArray();
  return napi_ok;
}

napi_status napi_get_array_length(napi_env e, napi_value v, uint32_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  // TODO: Should this also check to see if v is an array before blindly casting it?
  v8::Local<v8::Array> arr =
      v8impl::V8LocalValueFromJsValue(v).As<v8::Array>();

  *result = arr->Length();
  return napi_ok;
}

napi_status napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> a = v8impl::V8LocalValueFromJsValue(lhs);
  v8::Local<v8::Value> b = v8impl::V8LocalValueFromJsValue(rhs);

  *result = a->StrictEquals(b);
  return napi_ok;
}

napi_status napi_get_prototype(napi_env e, napi_value o, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, o);

  v8::Local<v8::Value> val = obj->GetPrototype();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return napi_ok;
}

napi_status napi_create_object(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result =v8impl::JsValueFromV8LocalValue(
    v8::Object::New(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_create_array(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Array::New(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_create_array_with_length(napi_env e, int length, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Array::New(v8impl::V8IsolateFromJsEnv(e), length));

  return napi_ok;
}

napi_status napi_create_string(napi_env e, const char* s, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, s);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return napi_ok;
}

napi_status napi_create_string_with_length(napi_env e,
                                           const char* s,
                                           size_t length,
                                           napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8_LEN(isolate, str, s, length);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return napi_ok;
}

napi_status napi_create_number(napi_env e, double v, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));

  return napi_ok;
}

napi_status napi_create_boolean(napi_env e, bool b, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Boolean::New(v8impl::V8IsolateFromJsEnv(e), b));

  return napi_ok;
}

napi_status napi_create_symbol(napi_env e, const char* s, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  if (s == nullptr) {
    *result = v8impl::JsValueFromV8LocalValue(v8::Symbol::New(isolate));
  } else {
    v8::Local<v8::String> string;
    CHECK_NEW_FROM_UTF8(isolate, string, s);

    *result = v8impl::JsValueFromV8LocalValue(v8::Symbol::New(isolate, string));
  }

  return napi_ok;
}

napi_status napi_create_error(napi_env e, napi_value msg, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Exception::Error(
          v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));

  return napi_ok;
}

napi_status napi_create_type_error(napi_env e, napi_value msg, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Exception::TypeError(
          v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));

  return napi_ok;
}

napi_status napi_get_type_of_value(napi_env e, napi_value vv, napi_valuetype* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(vv);

  if (v->IsNumber()) {
    *result = napi_number;
  } else if (v->IsString()) {
    *result = napi_string;
  } else if (v->IsFunction()) {
    // This test has to come before IsObject because IsFunction
    // implies IsObject
    *result = napi_function;
  } else if (v->IsObject()) {
    *result = napi_object;
  } else if (v->IsBoolean()) {
    *result = napi_boolean;
  } else if (v->IsUndefined()) {
    *result = napi_undefined;
  } else if (v->IsSymbol()) {
    *result = napi_symbol;
  } else if (v->IsNull()) {
    *result = napi_null;
  } else {
    *result = napi_object;   // Is this correct?
  }

  return napi_ok;
}

napi_status napi_get_undefined(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Undefined(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_get_null(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::Null(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_get_false(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::False(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_get_true(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
    v8::True(v8impl::V8IsolateFromJsEnv(e)));

  return napi_ok;
}

napi_status napi_get_cb_args_length(napi_env e, napi_callback_info cbinfo, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->ArgsLength();
  return napi_ok;
}

napi_status napi_is_construct_call(napi_env e, napi_callback_info cbinfo, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->IsConstructCall();
  return napi_ok;
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
napi_status napi_get_cb_args(napi_env e, napi_callback_info cbinfo,
                      napi_value* buffer, size_t bufferlength) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(buffer);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  info->Args(buffer, bufferlength);
  return napi_ok;
}

napi_status napi_get_cb_this(napi_env e, napi_callback_info cbinfo, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->This();
  return napi_ok;
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_status napi_get_cb_holder(
  napi_env e,
  napi_callback_info cbinfo,
  napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->Holder();
  return napi_ok;
}

napi_status napi_get_cb_data(napi_env e, napi_callback_info cbinfo, void** result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
    reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->Data();
  return napi_ok;
}

napi_status napi_call_function(napi_env e,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  std::vector<v8::Handle<v8::Value>> args(argc);
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Object> v8recv;
  CHECK_TO_OBJECT(context, v8recv, recv);

  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
  auto maybe = v8func->Call(context, v8recv, argc, args.data());

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_get_global(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  // TODO(ianhall): what if we need the global object from a different
  // context in the same isolate?
  // Should napi_env be the current context rather than the current isolate?
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  *result = v8impl::JsValueFromV8LocalValue(context->Global());

  return napi_ok;
}

napi_status napi_throw(napi_env e, napi_value error) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

  isolate->ThrowException(
      v8impl::V8LocalValueFromJsValue(error));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, msg);

  isolate->ThrowException(v8::Exception::Error(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_type_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, msg);

  isolate->ThrowException(v8::Exception::TypeError(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_get_number_from_value(napi_env e, napi_value v, double* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v)->NumberValue();

  return napi_ok;
}

napi_status napi_get_string_from_value(napi_env e,
                                       napi_value v,
                                       char* buf,
                                       const int buf_size,
                                       int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  napi_valuetype v_type;
  napi_status err = napi_get_type_of_value(e, v, &v_type);

  // Consider: Should we pre-emptively set the length to zero ?
  RETURN_STATUS_IF_FALSE(err == napi_ok, err);

  if (v_type == napi_number) {
    v8::String::Utf8Value str(v8impl::V8LocalValueFromJsValue(v));
    int len = str.length();
    if (buf_size > len) {
      memcpy(buf, *str, len);
      *result = 0;
    } else {
      memcpy(buf, *str, buf_size - 1);
      *result = len - buf_size + 1;
    }
  } else {
    int len = 0;
    err = napi_get_string_utf8_length(e, v, &len);
    RETURN_STATUS_IF_FALSE(err == napi_ok, err);

    int copied = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
        ->WriteUtf8(
            buf,
            buf_size,
            0,
            v8::String::REPLACE_INVALID_UTF8 |
            v8::String::PRESERVE_ONE_BYTE_NULL);
    // add one for null ending
    *result = len - copied + 1;
  }

  return napi_ok;
}

napi_status napi_get_value_int32(napi_env e, napi_value v, int32_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v)->Int32Value();

  return napi_ok;
}

napi_status napi_get_value_uint32(napi_env e, napi_value v, uint32_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v)->Uint32Value();

  return napi_ok;
}

napi_status napi_get_value_int64(napi_env e, napi_value v, int64_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v)->IntegerValue();

  return napi_ok;
}

napi_status napi_get_value_bool(napi_env e, napi_value v, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v)->BooleanValue();

  return napi_ok;
}

napi_status napi_get_string_length(napi_env e, napi_value v, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Length();

  return napi_ok;
}

napi_status napi_get_string_utf8_length(napi_env e, napi_value v, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()->Utf8Length();

  return napi_ok;
}

napi_status napi_get_string_utf8(napi_env e, napi_value v, char* buf, int bufsize, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::V8LocalValueFromJsValue(v).As<v8::String>()
      ->WriteUtf8(
          buf,
          bufsize,
          0,
          v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8);

  return napi_ok;
}

napi_status napi_coerce_to_object(napi_env e, napi_value v, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, v);

  *result = v8impl::JsValueFromV8LocalValue(obj);
  return napi_ok;
}

napi_status napi_coerce_to_string(napi_env e, napi_value v, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> str;

  CHECK_TO_STRING(context, str, v);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return napi_ok;
}

napi_status napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
                      napi_destruct destructor, napi_weakref* handle) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, jsObject);

  v8impl::ObjectWrapWrapper* wrap =
      new v8impl::ObjectWrapWrapper(obj, nativeObj, destructor);

  if (handle)
  {
    return napi_create_weakref(
      e,
      v8impl::JsValueFromV8LocalValue(wrap->handle()),
      handle);
  }
  else
  {
    // TODO: Is the handle parameter really optional?
    //       Why would anyone want to construct an object wrap and immediately lose it?
    return napi_ok;
  }
}

napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, jsObject);

  *result = v8impl::ObjectWrapWrapper::Unwrap(obj);
  return napi_ok;
}

napi_status napi_create_persistent(napi_env e, napi_value v, napi_persistent* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      new v8::Persistent<v8::Value>(
          isolate, v8impl::V8LocalValueFromJsValue(v));

  *result = v8impl::JsPersistentFromV8PersistentValue(thePersistent);
  return napi_ok;
}

napi_status napi_release_persistent(napi_env e, napi_persistent p) {
  NAPI_PREAMBLE(e);

  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  thePersistent->Reset();
  delete thePersistent;

  return napi_ok;
}

napi_status napi_get_persistent_value(napi_env e, napi_persistent p, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsPersistentValue(p);
  v8::Local<v8::Value> napi_value =
      v8::Local<v8::Value>::New(isolate, *thePersistent);

  *result = v8impl::JsValueFromV8LocalValue(napi_value);
  return napi_ok;
}

napi_status napi_create_weakref(napi_env e, napi_value v, napi_weakref* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      new v8::Persistent<v8::Value>(
          isolate, v8impl::V8LocalValueFromJsValue(v));
  thePersistent->SetWeak(static_cast<int*>(nullptr), v8impl::WeakRefCallback,
                         v8::WeakCallbackType::kParameter);
  // need to mark independent?
  *result = v8impl::JsWeakRefFromV8PersistentValue(thePersistent);
  return napi_ok;
}

napi_status napi_get_weakref_value(napi_env e, napi_weakref w, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsWeakRefValue(w);
  v8::Local<v8::Value> v =
      v8::Local<v8::Value>::New(isolate, *thePersistent);
  if (v.IsEmpty()) {
    *result = nullptr;
    return napi_ok;
  }
  *result = v8impl::JsValueFromV8LocalValue(v);
  return napi_ok;
}

napi_status napi_release_weakref(napi_env e, napi_weakref w) {
  NAPI_PREAMBLE(e);

  v8::Persistent<v8::Value> *thePersistent =
      v8impl::V8PersistentValueFromJsWeakRefValue(w);

  thePersistent->Reset();
  delete thePersistent;

  return napi_ok;
}

napi_status napi_open_handle_scope(napi_env e, napi_handle_scope* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsHandleScopeFromV8HandleScope(
      new v8impl::HandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
  return napi_ok;
}

napi_status napi_close_handle_scope(napi_env e, napi_handle_scope scope) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);

  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
  return napi_ok;
}

 napi_status napi_open_escapable_handle_scope(napi_env e,
                                              napi_escapable_handle_scope* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result =v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
      new v8impl::EscapableHandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
  return napi_ok;
}

 napi_status napi_close_escapable_handle_scope(napi_env e,
                                               napi_escapable_handle_scope scope) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);

  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  return napi_ok;
}

 napi_status napi_escape_handle(napi_env e, napi_escapable_handle_scope scope,
                              napi_value escapee, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);
  CHECK_ARG(result);

  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  *result = v8impl::JsValueFromV8LocalValue(
      s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
  return napi_ok;
}

napi_status napi_new_instance(napi_env e,
                              napi_value cons,
                              int argc,
                              napi_value* argv,
                              napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Local<v8::Function> v8cons = v8impl::V8LocalFunctionFromJsValue(cons);

  auto maybe = v8cons->NewInstance(context, argc, args.data());
  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_instanceof(napi_env e, napi_value obj, napi_value cons, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = false;

  v8::Local<v8::Object> v8Cons;
  v8::Local<v8::String> prototypeString;
  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  CHECK_TO_OBJECT(context, v8Cons, cons);

  if (!v8Cons->IsFunction()) {
    napi_throw_type_error(e, "constructor must be a function");

    return napi_set_last_error(napi_function_expected);
  }

  CHECK_NEW_FROM_UTF8(isolate, prototypeString, "prototype");

  auto maybe = v8Cons->Get(context, prototypeString);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  v8::Local<v8::Value> prototypeProperty = maybe.ToLocalChecked();

  if (!prototypeProperty->IsObject()) {
    napi_throw_type_error(e, "constructor prototype must be an object");

    return napi_set_last_error(napi_object_expected);
  }

  v8::Local<v8::Value> v8Obj = v8impl::V8LocalValueFromJsValue(obj);
  if (!v8Obj->StrictEquals(v8Cons)) {
    for (v8::Local<v8::Value> originalObj = v8Obj;
        !(v8Obj->IsNull() || v8Obj->IsUndefined());
        ) {
      if (v8Obj->StrictEquals(v8Cons)) {
        *result =
          !(originalObj->IsNumber() ||
            originalObj->IsBoolean() ||
            originalObj->IsString());
        break;
      }
      v8::Local<v8::Object> obj;
      CHECK_TO_OBJECT(context, obj, v8impl::JsValueFromV8LocalValue(v8Obj));
      v8Obj = obj->GetPrototype();
    }
  }

  return napi_ok;
}

napi_status napi_make_external(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  // v8impl::TryCatch doesn't make sense here since we're not calling into the
  // engine at all.
  *result = v;
  return napi_ok;
}

napi_status napi_make_callback(napi_env e,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> v8recv =
      v8impl::V8LocalValueFromJsValue(recv).As<v8::Object>();
  v8::Local<v8::Function> v8func =
      v8impl::V8LocalValueFromJsValue(func).As<v8::Function>();
  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Handle<v8::Value> retval =
    node::MakeCallback(isolate, v8recv, v8func, argc, args.data());
  *result = v8impl::JsValueFromV8LocalValue(retval);

  return napi_ok;
}

// Methods to support catching exceptions
napi_status napi_is_exception_pending(napi_env e, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = !v8impl::TryCatch::lastException().IsEmpty();
  return napi_ok;
}

napi_status napi_get_and_clear_last_exception(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  // TODO: Is there a chance that an exception will be thrown in the process of
  // attempting to retrieve the global static exception?
  if (v8impl::TryCatch::lastException().IsEmpty()) {
    return napi_get_undefined(e, result);
  } else {
    *result = v8impl::JsValueFromV8LocalValue(
      v8impl::TryCatch::lastException().Get(v8impl::V8IsolateFromJsEnv(e)));
    v8impl::TryCatch::lastException().Reset();
  }

  return napi_ok;
}

napi_status napi_buffer_new(napi_env e, char* data, uint32_t size, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto maybe = node::Buffer::New(v8impl::V8IsolateFromJsEnv(e), data, size);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_buffer_copy(napi_env e, const char* data,
                             uint32_t size, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto maybe = node::Buffer::Copy(v8impl::V8IsolateFromJsEnv(e), data, size);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_buffer_has_instance(napi_env e, napi_value v, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(v));
  return napi_ok;
}

napi_status napi_buffer_data(napi_env e, napi_value v, char** result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = node::Buffer::Data(v8impl::V8LocalValueFromJsValue(v).As<v8::Object>());
  return napi_ok;
}

napi_status napi_buffer_length(napi_env e, napi_value v, size_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = node::Buffer::Length(v8impl::V8LocalValueFromJsValue(v));
  return napi_ok;
}
