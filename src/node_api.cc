/******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * The current status is "Experimental" and should not be used for
 * production applications.  The API is still subject to change
 * and as an experimental feature is NOT subject to semver.
 *
 ******************************************************************************/


#include <node_buffer.h>
#include <node_object_wrap.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include "node_api_internal.h"

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
  explicit HandleScopeWrapper(v8::Isolate* isolate) : scope(isolate) {}

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
  explicit EscapableHandleScopeWrapper(v8::Isolate* isolate) : scope(isolate) {}
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

napi_escapable_handle_scope JsEscapableHandleScopeFromV8EscapableHandleScope(
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
    U(v8::Local<v8::Value> _l) : l(_l) {}
  } u(local);
  assert(sizeof(u.v) == sizeof(u.l));
  return u.v;
}

v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v) {
  // Likewise awkward
  union U {
    napi_value v;
    v8::Local<v8::Value> l;
    U(napi_value _v) : v(_v) {}
  } u(v);
  assert(sizeof(u.v) == sizeof(u.l));
  return u.l;
}

static v8::Local<v8::Function> V8LocalFunctionFromJsValue(napi_value v) {
  // Likewise awkward
  union U {
    napi_value v;
    v8::Local<v8::Function> f;
    U(napi_value _v) : v(_v) {}
  } u(v);
  assert(sizeof(u.v) == sizeof(u.f));
  return u.f;
}

// Wrapper around v8::Persistent that implements reference counting.
class Reference {
 public:
  Reference(v8::Isolate* isolate,
            v8::Local<v8::Value> value,
            int initialRefcount,
            bool deleteSelf,
            napi_finalize finalizeCallback = nullptr,
            void* finalizeData = nullptr,
            void* finalizeHint = nullptr)
       : _isolate(isolate),
        _persistent(isolate, value),
        _refcount(initialRefcount),
        _deleteSelf(deleteSelf),
        _finalizeCallback(finalizeCallback),
        _finalizeData(finalizeData),
        _finalizeHint(finalizeHint) {
    if (initialRefcount == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
      _persistent.MarkIndependent();
    }
  }

  ~Reference() {
    // The V8 Persistent class currently does not reset in its destructor:
    // see NonCopyablePersistentTraits::kResetInDestructor = false.
    // (Comments there claim that might change in the future.)
    // To avoid memory leaks, it is better to reset at this time, however
    // care must be taken to avoid attempting this after the Isolate has
    // shut down, for example via a static (atexit) destructor.
    _persistent.Reset();
  }

  int AddRef() {
    if (++_refcount == 1) {
      _persistent.ClearWeak();
    }

    return _refcount;
  }

  int Release() {
    if (--_refcount == 0) {
      _persistent.SetWeak(
          this, FinalizeCallback, v8::WeakCallbackType::kParameter);
      _persistent.MarkIndependent();
    }

    return _refcount;
  }

  v8::Local<v8::Value> Get() {
    if (_persistent.IsEmpty()) {
      return v8::Local<v8::Value>();
    } else {
      return v8::Local<v8::Value>::New(_isolate, _persistent);
    }
  }

 private:
  static void FinalizeCallback(const v8::WeakCallbackInfo<Reference>& data) {
    Reference* reference = data.GetParameter();
    reference->_persistent.Reset();

    // Check before calling the finalize callback, because the callback might
    // delete it.
    bool deleteSelf = reference->_deleteSelf;

    if (reference->_finalizeCallback != nullptr) {
      reference->_finalizeCallback(reference->_finalizeData,
          reference->_finalizeHint);
    }

    if (deleteSelf) {
      delete reference;
    }
  }

  v8::Isolate* _isolate;
  v8::Persistent<v8::Value> _persistent;
  int _refcount;
  bool _deleteSelf;
  napi_finalize _finalizeCallback;
  void* _finalizeData;
  void* _finalizeHint;
};

class TryCatch : public v8::TryCatch {
 public:
  explicit TryCatch(v8::Isolate* isolate)
      : v8::TryCatch(isolate), _isolate(isolate) {}

  ~TryCatch() {
    if (HasCaught()) {
      _theException.Reset(_isolate, Exception());
    }
  }

  static v8::Persistent<v8::Value>& lastException() { return _theException; }

 private:
  static v8::Persistent<v8::Value> _theException;
  v8::Isolate* _isolate;
};

v8::Persistent<v8::Value> TryCatch::_theException;

//=== Function napi_callback wrapper =================================

static const int kDataIndex = 0;

static const int kFunctionIndex = 1;
static const int kFunctionFieldCount = 2;

static const int kGetterIndex = 1;
static const int kSetterIndex = 2;
static const int kAccessorFieldCount = 3;

// Base class extended by classes that wrap V8 function and property callback
// info.
class CallbackWrapper {
 public:
  CallbackWrapper(napi_value thisArg, int argsLength, void* data)
      : _this(thisArg), _argsLength(argsLength), _data(data) {}

  virtual napi_value Holder() = 0;
  virtual bool IsConstructCall() = 0;
  virtual void Args(napi_value* buffer, int bufferlength) = 0;
  virtual void SetReturnValue(napi_value v) = 0;

  napi_value This() { return _this; }

  int ArgsLength() { return _argsLength; }

  void* Data() { return _data; }

 protected:
  const napi_value _this;
  const int _argsLength;
  void* _data;
};

template <typename T, int I>
class CallbackWrapperBase : public CallbackWrapper {
 public:
  CallbackWrapperBase(const T& cbinfo, const int argsLength)
      : CallbackWrapper(JsValueFromV8LocalValue(cbinfo.This()),
                        argsLength,
                        nullptr),
        _cbinfo(cbinfo),
        _cbdata(v8::Local<v8::Object>::Cast(cbinfo.Data())) {
    _data = v8::Local<v8::External>::Cast(_cbdata->GetInternalField(kDataIndex))
                ->Value();
  }

  /*virtual*/
  napi_value Holder() override {
    return JsValueFromV8LocalValue(_cbinfo.Holder());
  }

  /*virtual*/
  bool IsConstructCall() override { return false; }

 protected:
  void InvokeCallback() {
    napi_callback_info cbinfo_wrapper = reinterpret_cast<napi_callback_info>(
        static_cast<CallbackWrapper*>(this));
    napi_callback cb = reinterpret_cast<napi_callback>(
        v8::Local<v8::External>::Cast(_cbdata->GetInternalField(I))->Value());
    v8::Isolate* isolate = _cbinfo.GetIsolate();
    cb(v8impl::JsEnvFromV8Isolate(isolate), cbinfo_wrapper);

    if (!TryCatch::lastException().IsEmpty()) {
      isolate->ThrowException(
          v8::Local<v8::Value>::New(isolate, TryCatch::lastException()));
      TryCatch::lastException().Reset();
    }
  }

  const T& _cbinfo;
  const v8::Local<v8::Object> _cbdata;
};

class FunctionCallbackWrapper
    : public CallbackWrapperBase<v8::FunctionCallbackInfo<v8::Value>,
                                 kFunctionIndex> {
 public:
  static void Invoke(const v8::FunctionCallbackInfo<v8::Value>& info) {
    FunctionCallbackWrapper cbwrapper(info);
    cbwrapper.InvokeCallback();
  }

  explicit FunctionCallbackWrapper(
      const v8::FunctionCallbackInfo<v8::Value>& cbinfo)
      : CallbackWrapperBase(cbinfo, cbinfo.Length()) {}

  /*virtual*/
  bool IsConstructCall() override { return _cbinfo.IsConstructCall(); }

  /*virtual*/
  void Args(napi_value* buffer, int bufferlength) override {
    int i = 0;
    int min = std::min(bufferlength, _argsLength);

    for (; i < min; i += 1) {
      buffer[i] = v8impl::JsValueFromV8LocalValue(_cbinfo[i]);
    }

    if (i < bufferlength) {
      napi_value undefined =
          v8impl::JsValueFromV8LocalValue(v8::Undefined(_cbinfo.GetIsolate()));
      for (; i < bufferlength; i += 1) {
        buffer[i] = undefined;
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value v) override {
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    _cbinfo.GetReturnValue().Set(val);
  }
};

class GetterCallbackWrapper
    : public CallbackWrapperBase<v8::PropertyCallbackInfo<v8::Value>,
                                 kGetterIndex> {
 public:
  static void Invoke(v8::Local<v8::Name> property,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
    GetterCallbackWrapper cbwrapper(info);
    cbwrapper.InvokeCallback();
  }

  explicit GetterCallbackWrapper(
      const v8::PropertyCallbackInfo<v8::Value>& cbinfo)
      : CallbackWrapperBase(cbinfo, 0) {}

  /*virtual*/
  void Args(napi_value* buffer, int bufferlength) override {
    if (bufferlength > 0) {
      napi_value undefined =
          v8impl::JsValueFromV8LocalValue(v8::Undefined(_cbinfo.GetIsolate()));
      for (int i = 0; i < bufferlength; i += 1) {
        buffer[i] = undefined;
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value v) override {
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    _cbinfo.GetReturnValue().Set(val);
  }
};

class SetterCallbackWrapper
    : public CallbackWrapperBase<v8::PropertyCallbackInfo<void>, kSetterIndex> {
 public:
  static void Invoke(v8::Local<v8::Name> property,
                     v8::Local<v8::Value> v,
                     const v8::PropertyCallbackInfo<void>& info) {
    SetterCallbackWrapper cbwrapper(info, v);
    cbwrapper.InvokeCallback();
  }

  SetterCallbackWrapper(const v8::PropertyCallbackInfo<void>& cbinfo,
                        const v8::Local<v8::Value>& value)
      : CallbackWrapperBase(cbinfo, 1), _value(value) {}

  /*virtual*/
  void Args(napi_value* buffer, int bufferlength) override {
    if (bufferlength > 0) {
      buffer[0] = v8impl::JsValueFromV8LocalValue(_value);

      if (bufferlength > 1) {
        napi_value undefined = v8impl::JsValueFromV8LocalValue(
            v8::Undefined(_cbinfo.GetIsolate()));
        for (int i = 1; i < bufferlength; i += 1) {
          buffer[i] = undefined;
        }
      }
    }
  }

  /*virtual*/
  void SetReturnValue(napi_value v) override {
    // Cannot set the return value of a setter.
    assert(false);
  }

 private:
  const v8::Local<v8::Value>& _value;
};

// Creates an object to be made available to the static function callback
// wrapper, used to retrieve the native callback function and data pointer.
v8::Local<v8::Object> CreateFunctionCallbackData(napi_env e,
                                                 napi_callback cb,
                                                 void* data) {
  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
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
v8::Local<v8::Object> CreateAccessorCallbackData(napi_env e,
                                                 napi_callback getter,
                                                 napi_callback setter,
                                                 void* data) {
  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
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

// Intercepts the Node-V8 module registration callback. Converts parameters
// to NAPI equivalents and then calls the registration callback specified
// by the NAPI module.
void napi_module_register_cb(v8::Local<v8::Object> exports,
                             v8::Local<v8::Value> module,
                             v8::Local<v8::Context> context,
                             void* priv) {
  napi_module* mod = static_cast<napi_module*>(priv);
  mod->nm_register_func(
    v8impl::JsEnvFromV8Isolate(context->GetIsolate()),
    v8impl::JsValueFromV8LocalValue(exports),
    v8impl::JsValueFromV8LocalValue(module),
    mod->nm_priv);
}

#ifndef EXTERNAL_NAPI
namespace node {
  // Indicates whether NAPI was enabled/disabled via the node command-line.
  extern bool load_napi_modules;
}
#endif  // EXTERNAL_NAPI

// Registers a NAPI module.
void napi_module_register(napi_module* mod) {
  // NAPI modules always work with the current node version.
  int moduleVersion = NODE_MODULE_VERSION;

#ifndef EXTERNAL_NAPI
  if (!node::load_napi_modules) {
    // NAPI is disabled, so set the module version to -1 to cause the module
    // to be unloaded.
    moduleVersion = -1;
  }
#endif  // EXTERNAL_NAPI

  node::node_module* nm = new node::node_module {
    moduleVersion,
    mod->nm_flags,
    nullptr,
    mod->nm_filename,
    nullptr,
    napi_module_register_cb,
    mod->nm_modname,
    mod,  // priv
    nullptr,
  };
  node::node_module_register(nm);
}

#define RETURN_STATUS_IF_FALSE(condition, status)                       \
  do {                                                                  \
    if (!(condition)) {                                                 \
      return napi_set_last_error((status));                             \
    }                                                                   \
  } while (0)

#define CHECK_ARG(arg) RETURN_STATUS_IF_FALSE((arg), napi_invalid_arg)

#define CHECK_MAYBE_EMPTY(maybe, status) \
  RETURN_STATUS_IF_FALSE(!((maybe).IsEmpty()), (status))

#define CHECK_MAYBE_NOTHING(maybe, status) \
  RETURN_STATUS_IF_FALSE(!((maybe).IsNothing()), (status))

// NAPI_PREAMBLE is not wrapped in do..while: tryCatch must have function scope.
#define NAPI_PREAMBLE(e)                                              \
  CHECK_ARG(e);                                                       \
  RETURN_STATUS_IF_FALSE(v8impl::TryCatch::lastException().IsEmpty(), \
                         napi_pending_exception);                     \
  napi_clear_last_error();                                            \
  v8impl::TryCatch tryCatch(v8impl::V8IsolateFromJsEnv((e)))

#define CHECK_TO_TYPE(type, context, result, src, status)                     \
  do {                                                                        \
    auto maybe = v8impl::V8LocalValueFromJsValue((src))->To##type((context)); \
    CHECK_MAYBE_EMPTY(maybe, (status));                                       \
    result = maybe.ToLocalChecked();                                          \
  } while (0)

#define CHECK_TO_OBJECT(context, result, src) \
  CHECK_TO_TYPE(Object, context, result, src, napi_object_expected)

#define CHECK_TO_STRING(context, result, src) \
  CHECK_TO_TYPE(String, context, result, src, napi_string_expected)

#define CHECK_TO_NUMBER(context, result, src) \
  CHECK_TO_TYPE(Number, context, result, src, napi_number_expected)

#define CHECK_TO_BOOL(context, result, src) \
  CHECK_TO_TYPE(Boolean, context, result, src, napi_boolean_expected)

#define CHECK_NEW_FROM_UTF8_LEN(isolate, result, str, len)          \
  do {                                                              \
    auto str_maybe = v8::String::NewFromUtf8(                       \
        (isolate), (str), v8::NewStringType::kInternalized, (len)); \
    CHECK_MAYBE_EMPTY(str_maybe, napi_generic_failure);             \
    result = str_maybe.ToLocalChecked();                            \
  } while (0)

#define CHECK_NEW_FROM_UTF8(isolate, result, str) \
  CHECK_NEW_FROM_UTF8_LEN((isolate), (result), (str), -1)

#define GET_RETURN_STATUS()        \
  (!tryCatch.HasCaught() ? napi_ok \
                         : napi_set_last_error(napi_pending_exception))

// Static last error returned from napi_get_last_error_info
napi_extended_error_info static_last_error = {};

// Warning: Keep in-sync with napi_status enum
const char* error_messages[] = {nullptr,
                                "Invalid pointer passed as argument",
                                "An object was expected",
                                "A string was expected",
                                "A function was expected",
                                "A number was expected",
                                "A boolean was expected",
                                "Unknown failure",
                                "An exception is pending"};

void napi_clear_last_error() {
  static_last_error.error_code = napi_ok;

  // TODO(boingoing): Should this be a callback?
  static_last_error.engine_error_code = 0;
  static_last_error.engine_reserved = nullptr;
}

const napi_extended_error_info* napi_get_last_error_info() {
  static_assert(
      sizeof(error_messages) / sizeof(*error_messages) == napi_status_last,
      "Count of error messages must match count of error values");
  assert(static_last_error.error_code < napi_status_last);

  // Wait until someone requests the last error information to fetch the error
  // message string
  static_last_error.error_message =
      error_messages[static_last_error.error_code];

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

napi_status napi_create_function(napi_env e,
                                 const char* utf8name,
                                 napi_callback cb,
                                 void* data,
                                 napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Function> retval;

  v8::EscapableHandleScope scope(isolate);

  v8::Local<v8::Object> cbdata =
      v8impl::CreateFunctionCallbackData(e, cb, data);

  RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  retval = scope.Escape(tpl->GetFunction());

  if (utf8name) {
    v8::Local<v8::String> namestring;
    CHECK_NEW_FROM_UTF8(isolate, namestring, utf8name);
    retval->SetName(namestring);
  }

  *result = v8impl::JsValueFromV8LocalValue(retval);

  return GET_RETURN_STATUS();
}

napi_status napi_define_class(napi_env e,
                              const char* utf8name,
                              napi_callback constructor,
                              void* data,
                              int property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);

  v8::EscapableHandleScope scope(isolate);
  v8::Local<v8::Object> cbdata =
      v8impl::CreateFunctionCallbackData(e, constructor, data);

  RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

  v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(
      isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

  // we need an internal field to stash the wrapped object
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::String> namestring;
  CHECK_NEW_FROM_UTF8(isolate, namestring, utf8name);
  tpl->SetClassName(namestring);

  int staticPropertyCount = 0;
  for (int i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = properties + i;

    if ((p->attributes & napi_static_property) != 0) {
      // Static properties are handled separately below.
      staticPropertyCount++;
      continue;
    }

    v8::Local<v8::String> propertyname;
    CHECK_NEW_FROM_UTF8(isolate, propertyname, p->utf8name);

    v8::PropertyAttribute attributes =
        static_cast<v8::PropertyAttribute>(p->attributes);

    // This code is similar to that in napi_define_property(); the
    // difference is it applies to a template instead of an object.
    if (p->method) {
      v8::Local<v8::Object> cbdata =
          v8impl::CreateFunctionCallbackData(e, p->method, p->data);

      RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

      v8::Local<v8::FunctionTemplate> t =
          v8::FunctionTemplate::New(isolate,
                                    v8impl::FunctionCallbackWrapper::Invoke,
                                    cbdata,
                                    v8::Signature::New(isolate, tpl));
      t->SetClassName(propertyname);

      tpl->PrototypeTemplate()->Set(propertyname, t, attributes);
    } else if (p->getter || p->setter) {
      v8::Local<v8::Object> cbdata =
          v8impl::CreateAccessorCallbackData(e, p->getter, p->setter, p->data);

      tpl->PrototypeTemplate()->SetAccessor(
          propertyname,
          v8impl::GetterCallbackWrapper::Invoke,
          p->setter ? v8impl::SetterCallbackWrapper::Invoke : nullptr,
          cbdata,
          v8::AccessControl::DEFAULT,
          attributes);
    } else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);
      tpl->PrototypeTemplate()->Set(propertyname, value, attributes);
    }
  }

  *result = v8impl::JsValueFromV8LocalValue(scope.Escape(tpl->GetFunction()));

  if (staticPropertyCount > 0) {
    std::vector<napi_property_descriptor> staticDescriptors;
    staticDescriptors.reserve(staticPropertyCount);

    for (int i = 0; i < property_count; i++) {
      const napi_property_descriptor* p = properties + i;
      if ((p->attributes & napi_static_property) != 0) {
        staticDescriptors.push_back(*p);
      }
    }

    napi_status status =
        napi_define_properties(e,
                               *result,
                               static_cast<int>(staticDescriptors.size()),
                               staticDescriptors.data());
    if (status != napi_ok) return status;
  }

  return GET_RETURN_STATUS();
}

napi_status napi_set_return_value(napi_env e,
                                  napi_callback_info cbinfo,
                                  napi_value value) {
  NAPI_PREAMBLE(e);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  info->SetReturnValue(value);
  return GET_RETURN_STATUS();
}

napi_status napi_get_propertynames(napi_env e,
                                   napi_value object,
                                   napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, object);

  auto maybe_propertynames = obj->GetPropertyNames(context);

  CHECK_MAYBE_EMPTY(maybe_propertynames, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(
      maybe_propertynames.ToLocalChecked());
  return GET_RETURN_STATUS();
}

napi_status napi_set_property(napi_env e,
                              napi_value object,
                              napi_value key,
                              napi_value value) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  v8::Maybe<bool> set_maybe = obj->Set(context, k, val);

  RETURN_STATUS_IF_FALSE(set_maybe.FromMaybe(false), napi_generic_failure);
  return GET_RETURN_STATUS();
}

napi_status napi_has_property(napi_env e,
                              napi_value object,
                              napi_value key,
                              bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Maybe<bool> has_maybe = obj->Has(context, k);

  CHECK_MAYBE_NOTHING(has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS();
}

napi_status napi_get_property(napi_env e,
                              napi_value object,
                              napi_value key,
                              napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> k = v8impl::V8LocalValueFromJsValue(key);
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  auto get_maybe = obj->Get(context, k);

  CHECK_MAYBE_EMPTY(get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS();
}

napi_status napi_set_named_property(napi_env e,
                                    napi_value object,
                                    const char* utf8name,
                                    napi_value value) {
  NAPI_PREAMBLE(e);

  v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(isolate, key, utf8name);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  v8::Maybe<bool> set_maybe = obj->Set(context, key, val);

  RETURN_STATUS_IF_FALSE(set_maybe.FromMaybe(false), napi_generic_failure);
  return GET_RETURN_STATUS();
}

napi_status napi_has_named_property(napi_env e,
                                    napi_value object,
                                    const char* utf8name,
                                    bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(isolate, key, utf8name);

  v8::Maybe<bool> has_maybe = obj->Has(context, key);

  CHECK_MAYBE_NOTHING(has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS();
}

napi_status napi_get_named_property(napi_env e,
                                    napi_value object,
                                    const char* utf8name,
                                    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Name> key;
  CHECK_NEW_FROM_UTF8(isolate, key, utf8name);

  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  auto get_maybe = obj->Get(context, key);

  CHECK_MAYBE_EMPTY(get_maybe, napi_generic_failure);

  v8::Local<v8::Value> val = get_maybe.ToLocalChecked();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS();
}

napi_status napi_set_element(napi_env e,
                             napi_value object,
                             uint32_t index,
                             napi_value value) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  auto set_maybe = obj->Set(context, index, val);

  RETURN_STATUS_IF_FALSE(set_maybe.FromMaybe(false), napi_generic_failure);

  return GET_RETURN_STATUS();
}

napi_status napi_has_element(napi_env e,
                             napi_value object,
                             uint32_t index,
                             bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  v8::Maybe<bool> has_maybe = obj->Has(context, index);

  CHECK_MAYBE_NOTHING(has_maybe, napi_generic_failure);

  *result = has_maybe.FromMaybe(false);
  return GET_RETURN_STATUS();
}

napi_status napi_get_element(napi_env e,
                             napi_value object,
                             uint32_t index,
                             napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;

  CHECK_TO_OBJECT(context, obj, object);

  auto get_maybe = obj->Get(context, index);

  CHECK_MAYBE_EMPTY(get_maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(get_maybe.ToLocalChecked());
  return GET_RETURN_STATUS();
}

napi_status napi_define_properties(napi_env e,
                                   napi_value object,
                                   int property_count,
                                   const napi_property_descriptor* properties) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj =
      v8impl::V8LocalValueFromJsValue(object).As<v8::Object>();

  for (int i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = &properties[i];

    v8::Local<v8::Name> name;
    CHECK_NEW_FROM_UTF8(isolate, name, p->utf8name);

    v8::PropertyAttribute attributes = static_cast<v8::PropertyAttribute>(
        p->attributes & ~napi_static_property);

    if (p->method) {
      v8::Local<v8::Object> cbdata =
          v8impl::CreateFunctionCallbackData(e, p->method, p->data);

      RETURN_STATUS_IF_FALSE(!cbdata.IsEmpty(), napi_generic_failure);

      v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(
          isolate, v8impl::FunctionCallbackWrapper::Invoke, cbdata);

      auto define_maybe =
          obj->DefineOwnProperty(context, name, t->GetFunction(), attributes);

      // IsNothing seems like a serious failure,
      // should we return a different error code if the define failed?
      if (define_maybe.IsNothing() || !define_maybe.FromMaybe(false)) {
        return napi_set_last_error(napi_generic_failure);
      }
    } else if (p->getter || p->setter) {
      v8::Local<v8::Object> cbdata =
          v8impl::CreateAccessorCallbackData(e, p->getter, p->setter, p->data);

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
      if (set_maybe.IsNothing() || !set_maybe.FromMaybe(false)) {
        return napi_set_last_error(napi_generic_failure);
      }
    } else {
      v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(p->value);

      auto define_maybe =
          obj->DefineOwnProperty(context, name, value, attributes);

      // IsNothing seems like a serious failure,
      // should we return a different error code if the define failed?
      if (define_maybe.IsNothing() || !define_maybe.FromMaybe(false)) {
        return napi_set_last_error(napi_generic_failure);
      }
    }
  }

  return GET_RETURN_STATUS();
}

napi_status napi_is_array(napi_env e, napi_value value, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);

  *result = val->IsArray();
  return GET_RETURN_STATUS();
}

napi_status napi_get_array_length(napi_env e,
                                  napi_value value,
                                  uint32_t* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  // TODO(boingoing): Should this also check to see if v is an array before
  // blindly casting it?
  v8::Local<v8::Array> arr =
    v8impl::V8LocalValueFromJsValue(value).As<v8::Array>();

  *result = arr->Length();
  return GET_RETURN_STATUS();
}

napi_status napi_strict_equals(napi_env e,
                               napi_value lhs,
                               napi_value rhs,
                               bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> a = v8impl::V8LocalValueFromJsValue(lhs);
  v8::Local<v8::Value> b = v8impl::V8LocalValueFromJsValue(rhs);

  *result = a->StrictEquals(b);
  return GET_RETURN_STATUS();
}

napi_status napi_get_prototype(napi_env e,
                               napi_value object,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, object);

  v8::Local<v8::Value> val = obj->GetPrototype();
  *result = v8impl::JsValueFromV8LocalValue(val);
  return GET_RETURN_STATUS();
}

napi_status napi_create_object(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Object::New(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

napi_status napi_create_array(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

napi_status napi_create_array_with_length(napi_env e,
                                          int length,
                                          napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Array::New(v8impl::V8IsolateFromJsEnv(e), length));

  return GET_RETURN_STATUS();
}

napi_status napi_create_string_utf8(napi_env e,
                                    const char* s,
                                    int length,
                                    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8_LEN(isolate, str, s, length);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return GET_RETURN_STATUS();
}

napi_status napi_create_string_utf16(napi_env e,
                                     const char16_t* s,
                                     int length,
                                     napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto isolate = v8impl::V8IsolateFromJsEnv(e);
  auto str_maybe =
      v8::String::NewFromTwoByte(isolate,
                                 reinterpret_cast<const uint16_t*>(s),
                                 v8::NewStringType::kInternalized,
                                 length);
  CHECK_MAYBE_EMPTY(str_maybe, napi_generic_failure);
  v8::Local<v8::String> str = str_maybe.ToLocalChecked();

  *result = v8impl::JsValueFromV8LocalValue(str);
  return GET_RETURN_STATUS();
}

napi_status napi_create_number(napi_env e, double v, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));

  return GET_RETURN_STATUS();
}

napi_status napi_create_boolean(napi_env e, bool b, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Boolean::New(v8impl::V8IsolateFromJsEnv(e), b));

  return GET_RETURN_STATUS();
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

  return GET_RETURN_STATUS();
}

napi_status napi_create_error(napi_env e, napi_value msg, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::Error(
      v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));

  return GET_RETURN_STATUS();
}

napi_status napi_create_type_error(napi_env e,
                                   napi_value msg,
                                   napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::TypeError(
      v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));

  return GET_RETURN_STATUS();
}

napi_status napi_create_range_error(napi_env e,
                                    napi_value msg,
                                    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(v8::Exception::RangeError(
      v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));

  return GET_RETURN_STATUS();
}

napi_status napi_get_type_of_value(napi_env e,
                                   napi_value value,
                                   napi_valuetype* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(value);

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
  } else if (v->IsExternal()) {
    *result = napi_external;
  } else {
    *result = napi_object;  // Is this correct?
  }

  return napi_ok;
}

napi_status napi_get_undefined(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsValueFromV8LocalValue(
      v8::Undefined(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

napi_status napi_get_null(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result =
      v8impl::JsValueFromV8LocalValue(v8::Null(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

napi_status napi_get_false(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result =
      v8impl::JsValueFromV8LocalValue(v8::False(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

napi_status napi_get_true(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result =
      v8impl::JsValueFromV8LocalValue(v8::True(v8impl::V8IsolateFromJsEnv(e)));

  return GET_RETURN_STATUS();
}

// Gets all callback info in a single call. (Ugly, but faster.)
napi_status napi_get_cb_info(
    napi_env e,                 // [in] NAPI environment handle
    napi_callback_info cbinfo,  // [in] Opaque callback-info handle
    int* argc,         // [in-out] Specifies the size of the provided argv array
                       // and receives the actual count of args.
    napi_value* argv,  // [out] Array of values
    napi_value* thisArg,  // [out] Receives the JS 'this' arg for the call
    void** data) {        // [out] Receives the data pointer for the callback.
  CHECK_ARG(argc);
  CHECK_ARG(argv);
  CHECK_ARG(thisArg);
  CHECK_ARG(data);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  info->Args(argv, std::min(*argc, info->ArgsLength()));
  *argc = info->ArgsLength();
  *thisArg = info->This();
  *data = info->Data();

  return napi_ok;
}

napi_status napi_get_cb_args_length(napi_env e,
                                    napi_callback_info cbinfo,
                                    int* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->ArgsLength();
  return napi_ok;
}

napi_status napi_is_construct_call(napi_env e,
                                   napi_callback_info cbinfo,
                                   bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->IsConstructCall();
  return napi_ok;
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
napi_status napi_get_cb_args(napi_env e,
                             napi_callback_info cbinfo,
                             napi_value* buffer,
                             int bufferlength) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ARG(buffer);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  info->Args(buffer, bufferlength);
  return napi_ok;
}

napi_status napi_get_cb_this(napi_env e,
                             napi_callback_info cbinfo,
                             napi_value* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->This();
  return napi_ok;
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_status napi_get_cb_holder(napi_env e,
                               napi_callback_info cbinfo,
                               napi_value* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
  CHECK_ARG(result);

  v8impl::CallbackWrapper* info =
      reinterpret_cast<v8impl::CallbackWrapper*>(cbinfo);

  *result = info->Holder();
  return napi_ok;
}

napi_status napi_get_cb_data(napi_env e,
                             napi_callback_info cbinfo,
                             void** result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because no V8 APIs are called.
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
                               const napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  std::vector<v8::Handle<v8::Value>> args(argc);
  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Handle<v8::Value> v8recv = v8impl::V8LocalValueFromJsValue(recv);

  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Local<v8::Function> v8func = v8impl::V8LocalFunctionFromJsValue(func);
  auto maybe = v8func->Call(context, v8recv, argc, args.data());

  if (tryCatch.HasCaught()) {
    return napi_set_last_error(napi_pending_exception);
  } else {
    CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);
    *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
    return napi_ok;
  }
}

napi_status napi_get_global(napi_env e, napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  // TODO(ianhall): what if we need the global object from a different
  // context in the same isolate?
  // Should napi_env be the current context rather than the current isolate?
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  *result = v8impl::JsValueFromV8LocalValue(context->Global());

  return GET_RETURN_STATUS();
}

napi_status napi_throw(napi_env e, napi_value error) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);

  isolate->ThrowException(v8impl::V8LocalValueFromJsValue(error));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, msg);

  isolate->ThrowException(v8::Exception::Error(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_type_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, msg);

  isolate->ThrowException(v8::Exception::TypeError(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_throw_range_error(napi_env e, const char* msg) {
  NAPI_PREAMBLE(e);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::String> str;
  CHECK_NEW_FROM_UTF8(isolate, str, msg);

  isolate->ThrowException(v8::Exception::RangeError(str));
  // any VM calls after this point and before returning
  // to the javascript invoker will fail
  return napi_ok;
}

napi_status napi_is_error(napi_env e, napi_value value, bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  *result = val->IsNativeError();

  return napi_ok;
}

napi_status napi_get_value_double(napi_env e,
                                  napi_value value,
                                  double* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsNumber(), napi_number_expected);

  *result = val.As<v8::Number>()->Value();

  return napi_ok;
}

napi_status napi_get_value_int32(napi_env e,
                                 napi_value value,
                                 int32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsNumber(), napi_number_expected);

  *result = val.As<v8::Int32>()->Value();

  return napi_ok;
}

napi_status napi_get_value_uint32(napi_env e,
                                  napi_value value,
                                  uint32_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsNumber(), napi_number_expected);

  *result = val.As<v8::Uint32>()->Value();

  return napi_ok;
}

napi_status napi_get_value_int64(napi_env e,
                                 napi_value value,
                                 int64_t* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsNumber(), napi_number_expected);

  *result = val.As<v8::Integer>()->Value();

  return napi_ok;
}

napi_status napi_get_value_bool(napi_env e, napi_value value, bool* result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsBoolean(), napi_boolean_expected);

  *result = val.As<v8::Boolean>()->Value();

  return napi_ok;
}

// Gets the number of CHARACTERS in the string.
napi_status napi_get_value_string_length(napi_env e,
                                         napi_value value,
                                         int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsString(), napi_string_expected);

  *result = val.As<v8::String>()->Length();

  return GET_RETURN_STATUS();
}

// Gets the number of BYTES in the UTF-8 encoded representation of the string.
napi_status napi_get_value_string_utf8_length(napi_env e,
                                              napi_value value,
                                              int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsString(), napi_string_expected);

  *result = val.As<v8::String>()->Utf8Length();

  return GET_RETURN_STATUS();
}

// Copies a JavaScript string into a UTF-8 string buffer. The result is the
// number
// of bytes copied into buf, including the null terminator. If the buf size is
// insufficient, the string will be truncated, including a null terminator.
napi_status napi_get_value_string_utf8(napi_env e,
                                       napi_value value,
                                       char* buf,
                                       int bufsize,
                                       int* result) {
  NAPI_PREAMBLE(e);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsString(), napi_string_expected);

  int copied = val.As<v8::String>()->WriteUtf8(
      buf, bufsize, nullptr, v8::String::REPLACE_INVALID_UTF8);

  if (result != nullptr) {
    *result = copied;
  }

  return GET_RETURN_STATUS();
}

// Gets the number of 2-byte code units in the UTF-16 encoded representation of
// the string.
napi_status napi_get_value_string_utf16_length(napi_env e,
                                               napi_value value,
                                               int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsString(), napi_string_expected);

  // V8 assumes UTF-16 length is the same as the number of characters.
  *result = val.As<v8::String>()->Length();

  return GET_RETURN_STATUS();
}

// Copies a JavaScript string into a UTF-16 string buffer. The result is the
// number
// of 2-byte code units copied into buf, including the null terminator. If the
// buf
// size is insufficient, the string will be truncated, including a null
// terminator.
napi_status napi_get_value_string_utf16(napi_env e,
                                        napi_value value,
                                        char16_t* buf,
                                        int bufsize,
                                        int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsString(), napi_string_expected);

  int copied = val.As<v8::String>()->Write(
      reinterpret_cast<uint16_t*>(buf), 0, bufsize, v8::String::NO_OPTIONS);

  if (result != nullptr) {
    *result = copied;
  }

  return GET_RETURN_STATUS();
}

napi_status napi_coerce_to_object(napi_env e,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> obj;
  CHECK_TO_OBJECT(context, obj, value);

  *result = v8impl::JsValueFromV8LocalValue(obj);
  return GET_RETURN_STATUS();
}

napi_status napi_coerce_to_bool(napi_env e,
                                napi_value value,
                                napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Boolean> b;

  CHECK_TO_BOOL(context, b, value);

  *result = v8impl::JsValueFromV8LocalValue(b);
  return GET_RETURN_STATUS();
}

napi_status napi_coerce_to_number(napi_env e,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Number> num;

  CHECK_TO_NUMBER(context, num, value);

  *result = v8impl::JsValueFromV8LocalValue(num);
  return GET_RETURN_STATUS();
}

napi_status napi_coerce_to_string(napi_env e,
                                  napi_value value,
                                  napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::String> str;

  CHECK_TO_STRING(context, str, value);

  *result = v8impl::JsValueFromV8LocalValue(str);
  return GET_RETURN_STATUS();
}

napi_status napi_wrap(napi_env e,
                      napi_value jsObject,
                      void* nativeObj,
                      napi_finalize finalize_cb,
                      void* finalize_hint,
                      napi_ref* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(jsObject);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Object> obj =
      v8impl::V8LocalValueFromJsValue(jsObject).As<v8::Object>();

  // Only objects that were created from a NAPI constructor's prototype
  // via napi_define_class() can be (un)wrapped.
  RETURN_STATUS_IF_FALSE(obj->InternalFieldCount() > 0, napi_invalid_arg);

  obj->SetAlignedPointerInInternalField(0, nativeObj);

  if (result != nullptr) {
    // The returned reference should be deleted via napi_delete_reference()
    // ONLY in response to the finalize callback invocation. (If it is deleted
    // before then, then the finalize callback will never be invoked.)
    // Therefore a finalize callback is required when returning a reference.
    CHECK_ARG(finalize_cb);
    v8impl::Reference* reference = new v8impl::Reference(
        isolate, obj, 0, false, finalize_cb, nativeObj, finalize_hint);
    *result = reinterpret_cast<napi_ref>(reference);
  } else if (finalize_cb != nullptr) {
    // Create a self-deleting reference just for the finalize callback.
    new v8impl::Reference(
        isolate, obj, 0, true, finalize_cb, nativeObj, finalize_hint);
  }

  return GET_RETURN_STATUS();
}

napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(jsObject);
  CHECK_ARG(result);

  v8::Local<v8::Object> obj =
      v8impl::V8LocalValueFromJsValue(jsObject).As<v8::Object>();

  // Only objects that were created from a NAPI constructor's prototype
  // via napi_define_class() can be (un)wrapped.
  RETURN_STATUS_IF_FALSE(obj->InternalFieldCount() > 0, napi_invalid_arg);

  *result = obj->GetAlignedPointerFromInternalField(0);

  return napi_ok;
}

napi_status napi_create_external(napi_env e,
                                 void* data,
                                 napi_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);

  v8::Local<v8::Value> externalValue = v8::External::New(isolate, data);

  // The Reference object will delete itself after invoking the finalizer
  // callback.
  new v8impl::Reference(isolate,
      externalValue,
      0,
      true,
      finalize_cb,
      data,
      finalize_hint);

  *result = v8impl::JsValueFromV8LocalValue(externalValue);

  return GET_RETURN_STATUS();
}

napi_status napi_get_value_external(napi_env e,
                                    napi_value value,
                                    void** result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(value);
  CHECK_ARG(result);

  v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(value);
  RETURN_STATUS_IF_FALSE(val->IsExternal(), napi_invalid_arg);

  v8::Local<v8::External> externalValue = val.As<v8::External>();
  *result = externalValue->Value();

  return GET_RETURN_STATUS();
}

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
napi_status napi_create_reference(napi_env e,
                                  napi_value value,
                                  int initial_refcount,
                                  napi_ref* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);
  RETURN_STATUS_IF_FALSE(initial_refcount >= 0, napi_invalid_arg);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);

  v8impl::Reference* reference = new v8impl::Reference(
      isolate, v8impl::V8LocalValueFromJsValue(value), initial_refcount, false);

  *result = reinterpret_cast<napi_ref>(reference);
  return GET_RETURN_STATUS();
}

// Deletes a reference. The referenced value is released, and may be GC'd unless
// there
// are other references to it.
napi_status napi_delete_reference(napi_env e, napi_ref ref) {
  // Omit NAPI_PREAMBLE and GET_RETURN_STATUS because V8 calls here cannot throw
  // JS exceptions.
  CHECK_ARG(ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  delete reference;

  return napi_ok;
}

// Increments the reference count, optionally returning the resulting count.
// After this call the
// reference will be a strong reference because its refcount is >0, and the
// referenced object is
// effectively "pinned". Calling this when the refcount is 0 and the object
// is unavailable
// results in an error.
napi_status napi_reference_addref(napi_env e, napi_ref ref, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  int count = reference->AddRef();

  if (result != nullptr) {
    *result = count;
  }

  return GET_RETURN_STATUS();
}

// Decrements the reference count, optionally returning the resulting count. If
// the result is
// 0 the reference is now weak and the object may be GC'd at any time if there
// are no other
// references. Calling this when the refcount is already 0 results in an error.
napi_status napi_reference_release(napi_env e, napi_ref ref, int* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(ref);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  int count = reference->Release();
  if (count < 0) {
    return napi_set_last_error(napi_generic_failure);
  }

  if (result != nullptr) {
    *result = count;
  }

  return GET_RETURN_STATUS();
}

// Attempts to get a referenced value. If the reference is weak, the value might
// no longer be
// available, in that case the call is still successful but the result is NULL.
napi_status napi_get_reference_value(napi_env e,
                                     napi_ref ref,
                                     napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(ref);
  CHECK_ARG(result);

  v8impl::Reference* reference = reinterpret_cast<v8impl::Reference*>(ref);
  *result = v8impl::JsValueFromV8LocalValue(reference->Get());

  return GET_RETURN_STATUS();
}

napi_status napi_open_handle_scope(napi_env e, napi_handle_scope* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsHandleScopeFromV8HandleScope(
      new v8impl::HandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
  return GET_RETURN_STATUS();
}

napi_status napi_close_handle_scope(napi_env e, napi_handle_scope scope) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);

  delete v8impl::V8HandleScopeFromJsHandleScope(scope);
  return GET_RETURN_STATUS();
}

napi_status napi_open_escapable_handle_scope(
    napi_env e,
    napi_escapable_handle_scope* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = v8impl::JsEscapableHandleScopeFromV8EscapableHandleScope(
      new v8impl::EscapableHandleScopeWrapper(v8impl::V8IsolateFromJsEnv(e)));
  return GET_RETURN_STATUS();
}

napi_status napi_close_escapable_handle_scope(
    napi_env e,
    napi_escapable_handle_scope scope) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);

  delete v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  return GET_RETURN_STATUS();
}

napi_status napi_escape_handle(napi_env e,
                               napi_escapable_handle_scope scope,
                               napi_value escapee,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(scope);
  CHECK_ARG(result);

  v8impl::EscapableHandleScopeWrapper* s =
      v8impl::V8EscapableHandleScopeFromJsEscapableHandleScope(scope);
  *result = v8impl::JsValueFromV8LocalValue(
      s->Escape(v8impl::V8LocalValueFromJsValue(escapee)));
  return GET_RETURN_STATUS();
}

napi_status napi_new_instance(napi_env e,
                              napi_value constructor,
                              int argc,
                              const napi_value* argv,
                              napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  std::vector<v8::Handle<v8::Value>> args(argc);
  for (int i = 0; i < argc; i++) {
    args[i] = v8impl::V8LocalValueFromJsValue(argv[i]);
  }

  v8::Local<v8::Function> v8cons =
    v8impl::V8LocalFunctionFromJsValue(constructor);

  auto maybe = v8cons->NewInstance(context, argc, args.data());
  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS();
}

napi_status napi_instanceof(napi_env e,
                            napi_value object,
                            napi_value constructor,
                            bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = false;

  v8::Local<v8::Object> v8Cons;
  v8::Local<v8::String> prototypeString;
  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  CHECK_TO_OBJECT(context, v8Cons, constructor);

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

  v8Cons = prototypeProperty->ToObject();

  v8::Local<v8::Value> v8Obj = v8impl::V8LocalValueFromJsValue(object);
  if (!v8Obj->StrictEquals(v8Cons)) {
    for (v8::Local<v8::Value> originalObj = v8Obj;
         !(v8Obj->IsNull() || v8Obj->IsUndefined());) {
      if (v8Obj->StrictEquals(v8Cons)) {
        *result = !(originalObj->IsNumber() || originalObj->IsBoolean() ||
                    originalObj->IsString());
        break;
      }
      v8::Local<v8::Object> obj;
      CHECK_TO_OBJECT(context, obj, v8impl::JsValueFromV8LocalValue(v8Obj));
      v8Obj = obj->GetPrototype();
    }
  }

  return GET_RETURN_STATUS();
}

napi_status napi_make_callback(napi_env e,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               const napi_value* argv,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
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

  return GET_RETURN_STATUS();
}

// Methods to support catching exceptions
napi_status napi_is_exception_pending(napi_env e, bool* result) {
  // NAPI_PREAMBLE is not used here: this function must execute when there is a
  // pending exception.
  CHECK_ARG(e);
  CHECK_ARG(result);

  *result = !v8impl::TryCatch::lastException().IsEmpty();
  return napi_ok;
}

napi_status napi_get_and_clear_last_exception(napi_env e, napi_value* result) {
  // NAPI_PREAMBLE is not used here: this function must execute when there is a
  // pending exception.
  CHECK_ARG(e);
  CHECK_ARG(result);

  // TODO(boingoing): Is there a chance that an exception will be thrown in
  // the process of attempting to retrieve the global static exception?
  if (v8impl::TryCatch::lastException().IsEmpty()) {
    return napi_get_undefined(e, result);
  } else {
    *result = v8impl::JsValueFromV8LocalValue(v8::Local<v8::Value>::New(
        v8impl::V8IsolateFromJsEnv(e), v8impl::TryCatch::lastException()));
    v8impl::TryCatch::lastException().Reset();
  }

  return napi_ok;
}

napi_status napi_create_buffer(napi_env e,
                               size_t size,
                               void** data,
                               napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(data);
  CHECK_ARG(result);

  auto maybe = node::Buffer::New(v8impl::V8IsolateFromJsEnv(e), size);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  v8::Local<v8::Object> jsBuffer = maybe.ToLocalChecked();

  *result = v8impl::JsValueFromV8LocalValue(jsBuffer);
  *data = node::Buffer::Data(jsBuffer);

  return GET_RETURN_STATUS();
}

napi_status napi_create_external_buffer(napi_env e,
                                        size_t size,
                                        void* data,
                                        napi_finalize finalize_cb,
                                        void* finalize_hint,
                                        napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto maybe = node::Buffer::New(v8impl::V8IsolateFromJsEnv(e),
                                 static_cast<char*>(data),
                                 size,
                                 (node::Buffer::FreeCallback)finalize_cb,
                                 finalize_hint);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS();
}

napi_status napi_create_buffer_copy(napi_env e,
                                    const void* data,
                                    size_t size,
                                    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  auto maybe = node::Buffer::Copy(v8impl::V8IsolateFromJsEnv(e),
    static_cast<const char*>(data), size);

  CHECK_MAYBE_EMPTY(maybe, napi_generic_failure);

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return GET_RETURN_STATUS();
}

napi_status napi_is_buffer(napi_env e, napi_value value, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  *result = node::Buffer::HasInstance(v8impl::V8LocalValueFromJsValue(value));
  return GET_RETURN_STATUS();
}

napi_status napi_get_buffer_info(napi_env e,
                                 napi_value value,
                                 void** data,
                                 size_t* length) {
  NAPI_PREAMBLE(e);

  v8::Local<v8::Object> buffer =
      v8impl::V8LocalValueFromJsValue(value).As<v8::Object>();

  if (data) {
    *data = node::Buffer::Data(buffer);
  }
  if (length) {
    *length = node::Buffer::Length(buffer);
  }

  return GET_RETURN_STATUS();
}

napi_status napi_is_arraybuffer(napi_env e, napi_value value, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> v8value = v8impl::V8LocalValueFromJsValue(value);
  *result = v8value->IsArrayBuffer();

  return GET_RETURN_STATUS();
}

napi_status napi_create_arraybuffer(napi_env e,
                                    size_t byte_length,
                                    void** data,
                                    napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, byte_length);

  // Optionally return a pointer to the buffer's data, to avoid another call to
  // retreive it.
  if (data != nullptr) {
    *data = buffer->GetContents().Data();
  }

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS();
}

napi_status napi_create_external_arraybuffer(napi_env e,
                                             void* external_data,
                                             size_t byte_length,
                                             napi_finalize finalize_cb,
                                             void* finalize_hint,
                                             napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Isolate* isolate = v8impl::V8IsolateFromJsEnv(e);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, external_data, byte_length);

  if (finalize_cb != nullptr) {
    // Create a self-deleting weak reference that invokes the finalizer
    // callback.
    new v8impl::Reference(isolate,
        buffer,
        0,
        true,
        finalize_cb,
        external_data,
        finalize_hint);
  }

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return GET_RETURN_STATUS();
}

napi_status napi_get_arraybuffer_info(napi_env e,
                                      napi_value arraybuffer,
                                      void** data,
                                      size_t* byte_length) {
  NAPI_PREAMBLE(e);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(value->IsArrayBuffer(), napi_invalid_arg);

  v8::ArrayBuffer::Contents contents =
      value.As<v8::ArrayBuffer>()->GetContents();

  if (data != nullptr) {
    *data = contents.Data();
  }

  if (byte_length != nullptr) {
    *byte_length = contents.ByteLength();
  }

  return GET_RETURN_STATUS();
}

napi_status napi_is_typedarray(napi_env e, napi_value value, bool* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> v8value = v8impl::V8LocalValueFromJsValue(value);
  *result = v8value->IsTypedArray();

  return GET_RETURN_STATUS();
}

napi_status napi_create_typedarray(napi_env e,
                                   napi_typedarray_type type,
                                   size_t length,
                                   napi_value arraybuffer,
                                   size_t byte_offset,
                                   napi_value* result) {
  NAPI_PREAMBLE(e);
  CHECK_ARG(result);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(arraybuffer);
  RETURN_STATUS_IF_FALSE(value->IsArrayBuffer(), napi_invalid_arg);

  v8::Local<v8::ArrayBuffer> buffer = value.As<v8::ArrayBuffer>();
  v8::Local<v8::TypedArray> typedArray;

  switch (type) {
    case napi_int8:
      typedArray = v8::Int8Array::New(buffer, byte_offset, length);
      break;
    case napi_uint8:
      typedArray = v8::Uint8Array::New(buffer, byte_offset, length);
      break;
    case napi_uint8_clamped:
      typedArray = v8::Uint8ClampedArray::New(buffer, byte_offset, length);
      break;
    case napi_int16:
      typedArray = v8::Int16Array::New(buffer, byte_offset, length);
      break;
    case napi_uint16:
      typedArray = v8::Uint16Array::New(buffer, byte_offset, length);
      break;
    case napi_int32:
      typedArray = v8::Int32Array::New(buffer, byte_offset, length);
      break;
    case napi_uint32:
      typedArray = v8::Uint32Array::New(buffer, byte_offset, length);
      break;
    case napi_float32:
      typedArray = v8::Float32Array::New(buffer, byte_offset, length);
      break;
    case napi_float64:
      typedArray = v8::Float64Array::New(buffer, byte_offset, length);
      break;
    default:
      return napi_set_last_error(napi_invalid_arg);
  }

  *result = v8impl::JsValueFromV8LocalValue(typedArray);
  return GET_RETURN_STATUS();
}

napi_status napi_get_typedarray_info(napi_env e,
                                     napi_value typedarray,
                                     napi_typedarray_type* type,
                                     size_t* length,
                                     void** data,
                                     napi_value* arraybuffer,
                                     size_t* byte_offset) {
  NAPI_PREAMBLE(e);

  v8::Local<v8::Value> value = v8impl::V8LocalValueFromJsValue(typedarray);
  RETURN_STATUS_IF_FALSE(value->IsTypedArray(), napi_invalid_arg);

  v8::Local<v8::TypedArray> array = value.As<v8::TypedArray>();

  if (type != nullptr) {
    if (value->IsInt8Array()) {
      *type = napi_int8;
    } else if (value->IsUint8Array()) {
      *type = napi_uint8;
    } else if (value->IsUint8ClampedArray()) {
      *type = napi_uint8_clamped;
    } else if (value->IsInt16Array()) {
      *type = napi_int16;
    } else if (value->IsUint16Array()) {
      *type = napi_uint16;
    } else if (value->IsInt32Array()) {
      *type = napi_int32;
    } else if (value->IsUint32Array()) {
      *type = napi_uint32;
    } else if (value->IsFloat32Array()) {
      *type = napi_float32;
    } else if (value->IsFloat64Array()) {
      *type = napi_float64;
    }
  }

  if (length != nullptr) {
    *length = array->Length();
  }

  v8::Local<v8::ArrayBuffer> buffer = array->Buffer();
  if (data != nullptr) {
    *data = static_cast<uint8_t*>(buffer->GetContents().Data()) +
            array->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = v8impl::JsValueFromV8LocalValue(buffer);
  }

  if (byte_offset != nullptr) {
    *byte_offset = array->ByteOffset();
  }

  return GET_RETURN_STATUS();
}
