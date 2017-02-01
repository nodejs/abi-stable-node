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
*  - The jsrt implementation of the API is roughly hacked together with no
*    attention paid to error handling or fault tolerance.
*  - Error handling in general is not included in the API at this time.
*
******************************************************************************/

#include "node_jsvmapi_internal.h"
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <env.h>
#include <vector>
#include "ChakraCore.h"

#include "src/jsrtutils.h"

#ifndef CALLBACK
#define CALLBACK
#endif

void napi_module_register(void* mod) {
    node::node_module_register(mod);
}

//Callback Info struct as per JSRT native function.
struct CallbackInfo {
  bool isConstructCall;
  unsigned short argc;
  napi_value* argv;
  void* data;
  napi_value returnValue;
};

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
}  // end of namespace v8impl

void CHAKRA_CALLBACK JsObjectWrapWrapperBeforeCollectCallback(JsRef ref, void* callbackState);

class ObjectWrapWrapper : public node::ObjectWrap {
public:
  ObjectWrapWrapper(napi_value jsObject, void* nativeObj, napi_destruct destructor) {
    _destructor = destructor;
    _nativeObj = nativeObj;

    ObjectWrapWrapper::Wrap(jsObject, this);

    JsSetObjectBeforeCollectCallback(reinterpret_cast<JsRef>(jsObject), this, JsObjectWrapWrapperBeforeCollectCallback);
  }

  void* getValue() {
    return _nativeObj;
  }

  static void Wrap(napi_value jsObject, void* externalData) {
    JsSetExternalData(jsObject, externalData);
  }

  static void* Unwrap(napi_value jsObject) {
    ObjectWrapWrapper* wrapper = nullptr;

    JsGetExternalData(reinterpret_cast<JsValueRef>(jsObject), reinterpret_cast<void**>(&wrapper));

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

void CHAKRA_CALLBACK JsObjectWrapWrapperBeforeCollectCallback(JsRef ref, void* callbackState)
{
  ObjectWrapWrapper* wrapper = reinterpret_cast<ObjectWrapWrapper*>(callbackState);
  delete wrapper;
}

#define RETURN_STATUS_IF_FALSE(condition, status)                       \
  do {                                                                  \
    if (!(condition)) {                                                 \
      return napi_set_last_error((status));                             \
    }                                                                   \
  } while(0)

#define CHECK_ARG(arg)                                                  \
  RETURN_STATUS_IF_FALSE((arg), napi_invalid_arg)

#define CHECK_JSRT(expr)                                                \
  do {                                                                  \
    JsErrorCode err = (expr);                                           \
    if (err != JsNoError) return napi_set_last_error(err);              \
  } while(0)

// This does not call napi_set_last_error because the expression
// is assumed to be a NAPI function call that already did.
#define CHECK_NAPI(expr)                                                \
  do {                                                                  \
    napi_status status = (expr);                                        \
    if (status != napi_ok) return status;                               \
  } while(0)

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

napi_status napi_set_last_error(JsErrorCode jsError, void* engine_reserved = nullptr) {
  napi_status status;
  switch (jsError) {
    case JsNoError: status = napi_ok; break;
    case JsErrorNullArgument:
    case JsErrorInvalidArgument: status = napi_invalid_arg; break;
    case JsErrorPropertyNotString: status = napi_string_expected; break;
    case JsErrorArgumentNotObject: status = napi_object_expected; break;
    case JsErrorScriptException:
    case JsErrorInExceptionState: status = napi_pending_exception; break;
    default: status = napi_generic_failure; break;
  }

  static_last_error.error_code = status;
  static_last_error.engine_error_code = jsError;
  static_last_error.engine_reserved = engine_reserved;
  return status;
}

//Stub for now
napi_env napi_get_current_env() {
  return nullptr;
}

_Ret_maybenull_ JsValueRef CALLBACK CallbackWrapper(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState) {
  JsErrorCode error = JsNoError;
  JsValueRef undefinedValue;
  error = JsGetUndefinedValue(&undefinedValue);
  CallbackInfo cbInfo;
  cbInfo.isConstructCall = isConstructCall;
  cbInfo.argc = argumentCount;
  cbInfo.argv = reinterpret_cast<napi_value*>(arguments);
  error = JsGetExternalData(callee, &cbInfo.data);
  cbInfo.returnValue = reinterpret_cast<napi_value>(undefinedValue);
  napi_callback cb = reinterpret_cast<napi_callback>(callbackState);
  // TODO(tawoll): get environment pointer instead of nullptr?
  cb(nullptr, reinterpret_cast<napi_callback_info>(&cbInfo));
  return reinterpret_cast<JsValueRef>(cbInfo.returnValue);
}

napi_status napi_create_function(napi_env e, napi_callback cb, void* data, napi_value* result) {
  CHECK_ARG(result);

  JsValueRef function;
  CHECK_JSRT(JsCreateFunction(CallbackWrapper, (void*)cb, &function));

  if (data) {
    CHECK_JSRT(JsSetExternalData(function, data));
  }

  *result = reinterpret_cast<napi_value>(function);
  return napi_ok;
}

napi_status napi_create_constructor(napi_env e, const char* utf8name, napi_callback cb,
  void* data, int property_count, napi_property_descriptor* properties, napi_value* result) {
  CHECK_ARG(result);

  napi_value namestring;
  CHECK_NAPI(napi_create_string(e, utf8name, &namestring));
  JsValueRef constructor;
  CHECK_JSRT(JsCreateNamedFunction(namestring, CallbackWrapper, (void*)cb, &constructor));

  if (data) {
    CHECK_JSRT(JsSetExternalData(constructor, data));
  }

  JsPropertyIdRef pid = nullptr;
  JsValueRef prototype = nullptr;
  CHECK_JSRT(JsCreatePropertyIdUtf8("prototype", 10, &pid));
  CHECK_JSRT(JsGetProperty(constructor, pid, &prototype));

  CHECK_JSRT(JsCreatePropertyIdUtf8("constructor", 12, &pid));
  CHECK_JSRT(JsSetProperty(prototype, pid, constructor, false));

  for (int i = 0; i < property_count; i++) {
    CHECK_NAPI(napi_define_property(e, reinterpret_cast<napi_value>(prototype), &properties[i]));
  }

  *result = reinterpret_cast<napi_value>(constructor);
  return napi_ok;
}

napi_status napi_set_function_name(napi_env e, napi_value func,
  napi_propertyname name) {
  // TODO: Consider removing the napi_set_function_name API.
  // Chakra can only set a function name when creating the function.
  // So, add a name parameter to napi_create_function instead. 
  return napi_ok;
}

napi_status napi_set_return_value(napi_env e,
  napi_callback_info cbinfo, napi_value v) {
  CallbackInfo *info = (CallbackInfo*)cbinfo;
  info->returnValue = v;
  return napi_ok;
}

napi_status napi_property_name(napi_env e, const char* utf8name, napi_propertyname* result) {
  CHECK_ARG(result);
  JsPropertyIdRef propertyId;
  CHECK_JSRT(JsCreatePropertyIdUtf8(utf8name, strlen(utf8name), &propertyId));
  *result = reinterpret_cast<napi_propertyname>(propertyId);
  return napi_ok;
}

napi_status napi_get_propertynames(napi_env e, napi_value o, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef propertyNames;
  CHECK_JSRT(JsGetOwnPropertyNames(object, &propertyNames));
  *result = reinterpret_cast<napi_value>(propertyNames);
  return napi_ok;
}

napi_status napi_set_property(napi_env e, napi_value o,
  napi_propertyname k, napi_value v) {
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsSetProperty(object, propertyId, value, true));
  return napi_ok;
}

napi_status napi_define_property(napi_env e, napi_value o,
    napi_property_descriptor* p) {
  napi_value descriptor;
  CHECK_NAPI(napi_create_object(e, &descriptor));

  napi_propertyname configurableProperty;
  CHECK_NAPI(napi_property_name(e, "configurable", &configurableProperty));
  napi_value configurable;
  CHECK_NAPI(napi_create_boolean(e, !(p->attributes & napi_dont_delete), &configurable));
  CHECK_NAPI(napi_set_property(e, descriptor, configurableProperty, configurable));

  napi_propertyname enumerableProperty;
  CHECK_NAPI(napi_property_name(e, "enumerable", &enumerableProperty));
  napi_value enumerable;
  CHECK_NAPI(napi_create_boolean(e, !(p->attributes & napi_dont_enum), &enumerable));
  CHECK_NAPI(napi_set_property(e, descriptor, enumerableProperty, enumerable));

  if (p->method) {
    napi_propertyname valueProperty;
    CHECK_NAPI(napi_property_name(e, "value", &valueProperty));
    napi_value method;
    CHECK_NAPI(napi_create_function(e, p->method, p->data, &method));
    CHECK_NAPI(napi_set_property(e, descriptor, valueProperty, method));
  }
  else if (p->getter || p->setter) {
    if (p->getter) {
      napi_propertyname getProperty;
      CHECK_NAPI(napi_property_name(e, "get", &getProperty));
      napi_value getter;
      CHECK_NAPI(napi_create_function(e, p->getter, p->data, &getter));
      CHECK_NAPI(napi_set_property(e, descriptor, getProperty, getter));
    }

    if (p->setter) {
      napi_propertyname setProperty;
      CHECK_NAPI(napi_property_name(e, "set", &setProperty));
      napi_value setter;
      CHECK_NAPI(napi_create_function(e, p->setter, p->data, &setter));
      CHECK_NAPI(napi_set_property(e, descriptor, setProperty, setter));
    }
  }
  else {
    RETURN_STATUS_IF_FALSE(p->value != nullptr, napi_invalid_arg);

    napi_propertyname writableProperty;
    CHECK_NAPI(napi_property_name(e, "writable", &writableProperty));
    napi_value writable;
    CHECK_NAPI(napi_create_boolean(e, !(p->attributes & napi_read_only), &writable));
    CHECK_NAPI(napi_set_property(e, descriptor, writableProperty, writable));

    napi_propertyname valueProperty;
    CHECK_NAPI(napi_property_name(e, "value", &valueProperty));
    CHECK_NAPI(napi_set_property(e, descriptor, valueProperty, p->value));
  }

  napi_propertyname nameProperty;
  CHECK_NAPI(napi_property_name(e, p->utf8name, &nameProperty));
  bool result;
  CHECK_JSRT(JsDefineProperty(
    reinterpret_cast<JsValueRef>(o),
    reinterpret_cast<JsPropertyIdRef>(nameProperty),
    reinterpret_cast<JsValueRef>(descriptor),
    &result));
  return napi_ok;
}

napi_status napi_instanceof(napi_env e, napi_value o, napi_value c, bool* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef constructor = reinterpret_cast<JsValueRef>(c);

  // FIXME: Remove this type check when we switch to a version of Chakracore
  // where passing an integer into JsInstanceOf as the constructor parameter
  // does not cause a segfault. The need for this if-statement is removed in at
  // least Chakracore 1.4.0, but maybe in an earlier version too.
  napi_valuetype valuetype;
  CHECK_NAPI(napi_get_type_of_value(e, c, &valuetype));
  if (valuetype != napi_function) {
    napi_throw_type_error(e, "constructor must be a function");
    return napi_set_last_error(napi_invalid_arg);
  }

  CHECK_JSRT(JsInstanceOf(object, constructor, result));
  return napi_ok;
}

napi_status napi_has_property(napi_env e, napi_value o, napi_propertyname k, bool* result) {
  CHECK_ARG(result);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  CHECK_JSRT(JsHasProperty(object, propertyId, result));
  return napi_ok;
}

napi_status napi_get_property(napi_env e, napi_value o, napi_propertyname k, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  CHECK_JSRT(JsGetProperty(object, propertyId, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_set_element(napi_env e, napi_value o, uint32_t i, napi_value v) {
  JsValueRef index = nullptr;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsIntToNumber(i, &index));
  CHECK_JSRT(JsSetIndexedProperty(object, index, value));
  return napi_ok;
}

napi_status napi_has_element(napi_env e, napi_value o, uint32_t i, bool* result) {
  CHECK_ARG(result);
  JsValueRef index = nullptr;
  CHECK_JSRT(JsIntToNumber(i, &index));
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  CHECK_JSRT(JsHasIndexedProperty(object, index, result));
  return napi_ok;
}

napi_status napi_get_element(napi_env e, napi_value o, uint32_t i, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef index = nullptr;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  CHECK_JSRT(JsIntToNumber(i, &index));
  CHECK_JSRT(JsGetIndexedProperty(object, index, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_is_array(napi_env e, napi_value v, bool* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueType type = JsUndefined;
  CHECK_JSRT(JsGetValueType(value, &type));
  *result = (type == JsArray);
  return napi_ok;
}

napi_status napi_get_array_length(napi_env e, napi_value v, uint32_t* result) {
  CHECK_ARG(result);
  JsPropertyIdRef propertyIdRef;
  CHECK_JSRT(JsCreatePropertyIdUtf8("length", 7, &propertyIdRef));
  JsValueRef lengthRef;
  JsValueRef arrayRef = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsGetProperty(arrayRef, propertyIdRef, &lengthRef));
  double sizeInDouble;
  CHECK_JSRT(JsNumberToDouble(lengthRef, &sizeInDouble));
  *result = static_cast<unsigned int>(sizeInDouble);
  return napi_ok;
}

napi_status napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs, bool* result) {
  CHECK_ARG(result);
  JsValueRef object1 = reinterpret_cast<JsValueRef>(lhs);
  JsValueRef object2 = reinterpret_cast<JsValueRef>(rhs);
  CHECK_JSRT(JsStrictEquals(object1, object2, result));
  return napi_ok;
}

napi_status napi_get_prototype(napi_env e, napi_value o, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  CHECK_JSRT(JsGetPrototype(object, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_object(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsCreateObject(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_array(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  unsigned int length = 0;
  CHECK_JSRT(JsCreateArray(length, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_array_with_length(napi_env e, int length, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsCreateArray(length, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_string(napi_env e, const char* s, napi_value* result) {
  CHECK_ARG(result);
  size_t length = strlen(s);
  CHECK_JSRT(JsCreateStringUtf8((uint8_t*)s, length, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_string_with_length(napi_env e,
  const char* s, size_t length, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsCreateStringUtf8((uint8_t*)s, length, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_number(napi_env e, double v, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsDoubleToNumber(v, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_boolean(napi_env e, bool b, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsBoolToBoolean(b, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_symbol(napi_env e, const char* s, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef description = nullptr;
  if (s != nullptr) {
    CHECK_JSRT(JsCreateStringUtf8((uint8_t*)s, strlen(s), &description));
  }
  CHECK_JSRT(JsCreateSymbol(description, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_error(napi_env, napi_value msg, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef message = reinterpret_cast<JsValueRef>(msg);
  CHECK_JSRT(JsCreateError(message, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_type_error(napi_env, napi_value msg, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef message = reinterpret_cast<JsValueRef>(msg);
  CHECK_JSRT(JsCreateTypeError(message, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_type_of_value(napi_env e, napi_value vv, napi_valuetype* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(vv);
  JsValueType valueType = JsUndefined;
  CHECK_JSRT(JsGetValueType(value, &valueType));

  switch (valueType) {
    case JsUndefined: *result = napi_undefined; break;
    case JsNull: *result = napi_null; break;
    case JsNumber: *result = napi_number; break;
    case JsString: *result = napi_string; break;
    case JsBoolean: *result = napi_boolean; break;
    case JsFunction: *result = napi_function; break;
    case JsSymbol: *result = napi_symbol; break;
    default: *result = napi_object; break;
  }
  return napi_ok;
}

napi_status napi_get_undefined(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetUndefinedValue(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_null(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetNullValue(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_false(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetFalseValue(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_true(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetTrueValue(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_cb_args_length(napi_env e, napi_callback_info cbinfo, int* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = (info->argc) - 1;
  return napi_ok;
}

napi_status napi_is_construct_call(napi_env e, napi_callback_info cbinfo, bool* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->isConstructCall;
  return napi_ok;
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
napi_status napi_get_cb_args(napi_env e, napi_callback_info cbinfo,
  napi_value* buffer, size_t bufferlength) {
  CHECK_ARG(cbinfo);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);

  int i = 0;
  // size_t appropriate for the buffer length parameter?
  // Probably this API is not the way to go.
  int min =
    static_cast<int>(bufferlength) < (info->argc) - 1 ?
    static_cast<int>(bufferlength) : (info->argc) - 1;

  for (; i < min; i++) {
    buffer[i] = info->argv[i + 1];
  }

  if (i < static_cast<int>(bufferlength)) {
    napi_value undefined;
    CHECK_JSRT(JsGetUndefinedValue(reinterpret_cast<JsValueRef*>(&undefined)));
    for (; i < static_cast<int>(bufferlength); i += 1) {
      buffer[i] = undefined;
    }
  }

  return napi_ok;
}

napi_status napi_get_cb_this(napi_env e, napi_callback_info cbinfo, napi_value* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->argv[0];
  return napi_ok;
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_status napi_get_cb_holder(napi_env e, napi_callback_info cbinfo, napi_value* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->argv[0];
  return napi_ok;
}

napi_status napi_get_cb_data(napi_env e, napi_callback_info cbinfo, void** result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->data;
  return napi_ok;
}

napi_status napi_call_function(napi_env e, napi_value recv,
  napi_value func, int argc, napi_value* argv, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(recv);
  JsValueRef function = reinterpret_cast<JsValueRef>(func);
  std::vector<JsValueRef> args(argc+1);
  args[0] = object;
  for (int i = 0; i < argc; i++) {
    args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
  }
  CHECK_JSRT(JsCallFunction(function, args.data(), argc + 1, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_get_global(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetGlobalObject(reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_throw(napi_env e, napi_value error) {
  JsValueRef exception = reinterpret_cast<JsValueRef>(error);
  CHECK_JSRT(JsSetException(exception));
  return napi_ok;
}

napi_status napi_throw_error(napi_env e, const char* msg) {
  JsValueRef strRef;
  JsValueRef exception;
  size_t length = strlen(msg);
  CHECK_JSRT(JsCreateStringUtf8((uint8_t*)msg, length, &strRef));
  CHECK_JSRT(JsCreateError(strRef, &exception));
  CHECK_JSRT(JsSetException(exception));
  return napi_ok;
}

napi_status napi_throw_type_error(napi_env e, const char* msg) {
  JsValueRef strRef;
  JsValueRef exception;
  size_t length = strlen(msg);
  CHECK_JSRT(JsCreateStringUtf8((uint8_t*)msg, length, &strRef));
  CHECK_JSRT(JsCreateTypeError(strRef, &exception));
  CHECK_JSRT(JsSetException(exception));
  return napi_ok;
}

napi_status napi_get_number_from_value(napi_env e, napi_value v, double* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef numberValue = nullptr;
  double doubleValue = 0.0;
  CHECK_JSRT(JsConvertValueToNumber(value, &numberValue));
  CHECK_JSRT(JsNumberToDouble(numberValue, result));
  return napi_ok;
}

napi_status napi_get_string_from_value(napi_env e, napi_value v,
  char* buf, const int buf_size, int* remain) {
  CHECK_ARG(remain);
  int len = 0;
  size_t copied = 0;
  JsValueRef stringValue = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsGetStringLength(stringValue, &len));
  CHECK_JSRT(JsCopyStringUtf8(stringValue, (uint8_t*)buf, buf_size, &copied));
  // add one for null ending
  *remain = len - static_cast<int>(copied) + 1;
  return napi_ok;
}

napi_status napi_get_value_int32(napi_env e, napi_value v, int32_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT(JsConvertValueToNumber(value, &numberValue));
  CHECK_JSRT(JsNumberToInt(numberValue, &valueInt));
  *result = static_cast<int32_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_uint32(napi_env e, napi_value v, uint32_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT(JsConvertValueToNumber(value, &numberValue));
  CHECK_JSRT(JsNumberToInt(numberValue, &valueInt));
  *result = static_cast<uint32_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_int64(napi_env e, napi_value v, int64_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT(JsConvertValueToNumber(value, &numberValue));
  CHECK_JSRT(JsNumberToInt(numberValue, &valueInt));
  *result = static_cast<int64_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_bool(napi_env e, napi_value v, bool* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef booleanValue = nullptr;
  CHECK_JSRT(JsConvertValueToBoolean(value, &booleanValue));
  CHECK_JSRT(JsBooleanToBool(booleanValue, result));
  return napi_ok;
}

napi_status napi_get_string_length(napi_env e, napi_value v, int* result) {
  CHECK_ARG(result);
  JsValueRef stringValue = reinterpret_cast<JsValueRef>(v);
  int length = 0;
  CHECK_JSRT(JsGetStringLength(stringValue, result));
  return napi_ok;
}

napi_status napi_get_string_utf8_length(napi_env e, napi_value v, int* result) {
  CHECK_ARG(result);
  JsValueRef strRef = reinterpret_cast<JsValueRef>(v);
  int length = 0;
  CHECK_JSRT(JsGetStringLength(strRef, &length));
  return napi_ok;
}

napi_status napi_get_string_utf8(napi_env e, napi_value v, char* buf, int bufsize, int* length) {
  CHECK_ARG(length);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  size_t len = 0;
  CHECK_JSRT(JsCopyStringUtf8(value, (uint8_t*)buf, bufsize, &len));
  *length = static_cast<int>(len);
  return napi_ok;
}

napi_status napi_coerce_to_object(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsConvertValueToObject(value, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_coerce_to_string(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsConvertValueToString(value, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
  napi_destruct destructor, napi_weakref* handle) {
  new ObjectWrapWrapper(jsObject, nativeObj, destructor);

  if (handle != nullptr)
  {
    CHECK_NAPI(napi_create_weakref(e, jsObject, handle));
  }
  return napi_ok;
}

napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result) {
  CHECK_ARG(result);
  *result = ObjectWrapWrapper::Unwrap(jsObject);
  return napi_ok;
}

napi_status napi_create_persistent(napi_env e, napi_value v, napi_persistent* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  if (value) {
      CHECK_JSRT(JsAddRef(static_cast<JsRef>(value), nullptr));
  }
  *result = reinterpret_cast<napi_persistent>(value);
  return napi_ok;
}

napi_status napi_release_persistent(napi_env e, napi_persistent p) {
  JsValueRef thePersistent = reinterpret_cast<JsValueRef>(p);
  CHECK_JSRT(JsRelease(static_cast<JsRef>(thePersistent), nullptr));
  return napi_ok;
}

napi_status napi_get_persistent_value(napi_env e, napi_persistent p, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(p);
  *result = reinterpret_cast<napi_value>(value);
  return napi_ok;
}

napi_status napi_create_weakref(napi_env e, napi_value v, napi_weakref* result)
{
  CHECK_ARG(result);
  JsValueRef strongRef = reinterpret_cast<JsValueRef>(v);
  JsWeakRef weakRef = nullptr;
  CHECK_JSRT(JsCreateWeakReference(strongRef, &weakRef));
  CHECK_JSRT(JsAddRef(weakRef, nullptr));
  *result = reinterpret_cast<napi_weakref>(weakRef);
  return napi_ok;
}

napi_status napi_get_weakref_value(napi_env e, napi_weakref w, napi_value* result)
{
  CHECK_ARG(result);
  JsWeakRef weakRef = reinterpret_cast<JsWeakRef>(w);
  CHECK_JSRT(JsGetWeakReferenceValue(weakRef, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_release_weakref(napi_env e, napi_weakref w) {
  JsRef weakRef = reinterpret_cast<JsRef>(w);
  CHECK_JSRT(JsRelease(weakRef, nullptr));
  return napi_ok;
}

/*********Stub implementation of handle scope apis' for JSRT***********/
napi_status napi_open_handle_scope(napi_env, napi_handle_scope* result) {
  CHECK_ARG(result);
  *result = nullptr;
  return napi_ok;
}

napi_status napi_close_handle_scope(napi_env, napi_handle_scope) {
  return napi_ok;
}

napi_status napi_open_escapable_handle_scope(napi_env, napi_escapable_handle_scope* result) {
  CHECK_ARG(result);
  *result = nullptr;
  return napi_ok;
}

napi_status napi_close_escapable_handle_scope(napi_env, napi_escapable_handle_scope) {
  return napi_ok;
}

//This one will return escapee value as this is called from leveldown db.
napi_status napi_escape_handle(napi_env e, napi_escapable_handle_scope scope,
  napi_value escapee, napi_value* result) {
  CHECK_ARG(result);
  *result = escapee;
  return napi_ok;
}
/**************************************************************/

napi_status napi_new_instance(napi_env e, napi_value cons,
  int argc, napi_value *argv, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef function = reinterpret_cast<JsValueRef>(cons);
  std::vector<JsValueRef> args(argc + 1);
  CHECK_JSRT(JsGetUndefinedValue(&args[0]));
  for (int i = 0; i < argc; i++) {
    args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
  }
  CHECK_JSRT(JsConstructObject(function, args.data(), argc + 1,
    reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_make_external(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef externalObj;
  CHECK_JSRT(JsCreateExternalObject(NULL, NULL, &externalObj));
  CHECK_JSRT(JsSetPrototype(externalObj, reinterpret_cast<JsValueRef>(v)));
  *result = reinterpret_cast<napi_value>(externalObj);
  return napi_ok;
}

napi_status napi_make_callback(napi_env e, napi_value recv,
  napi_value func, int argc, napi_value* argv, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef object = reinterpret_cast<JsValueRef>(recv);
  JsValueRef function = reinterpret_cast<JsValueRef>(func);
  std::vector<JsValueRef> args(argc + 1);
  args[0] = object;
  for (int i = 0; i < argc; i++)
  {
	  args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
  }
  CHECK_JSRT(JsCallFunction(function, args.data(), argc + 1,
    reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

struct ArrayBufferFinalizeInfo {
  void *data;

  void Free() {
    free(data);
	 delete this;
  }
};

void CALLBACK ExternalArrayBufferFinalizeCallback(void *data)
{
	static_cast<ArrayBufferFinalizeInfo*>(data)->Free();
}

napi_status napi_buffer_new(napi_env e, char* data, uint32_t size, napi_value* result) {
  CHECK_ARG(result);
  // TODO(tawoll): Replace v8impl with jsrt-based version.

  v8::MaybeLocal<v8::Object> maybe = node::Buffer::New(
    v8impl::V8IsolateFromJsEnv(e), data, size);
  if (maybe.IsEmpty()) {
    return napi_generic_failure;
  }
  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_buffer_copy(napi_env e, const char* data, uint32_t size, napi_value* result) {
  CHECK_ARG(result);
  // TODO(tawoll): Implement node::Buffer in terms of napi to avoid using chakra shim here.

  v8::MaybeLocal<v8::Object> maybe = node::Buffer::Copy(
    v8impl::V8IsolateFromJsEnv(e), data, size);
  if (maybe.IsEmpty()) {
    return napi_generic_failure;
  }
  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_buffer_has_instance(napi_env e, napi_value v, bool* result) {
  CHECK_ARG(result);
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsTypedArrayType arrayType;
  CHECK_JSRT(JsGetTypedArrayInfo(typedArray, &arrayType, nullptr, nullptr, nullptr));
  *result = (arrayType == JsArrayTypeUint8);
  return napi_ok;
}

napi_status napi_buffer_data(napi_env e, napi_value v, char** result) {
  CHECK_ARG(result);
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsValueRef arrayBuffer;
  unsigned int byteOffset;
  ChakraBytePtr buffer;
  unsigned int bufferLength;
  CHECK_JSRT(JsGetTypedArrayInfo(typedArray, nullptr, &arrayBuffer, &byteOffset, nullptr));
  CHECK_JSRT(JsGetArrayBufferStorage(arrayBuffer, &buffer, &bufferLength));
  *result = reinterpret_cast<char*>(buffer + byteOffset);
  return napi_ok;
}

napi_status napi_buffer_length(napi_env e, napi_value v, size_t* result) {
  CHECK_ARG(result);
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  unsigned int len;
  CHECK_JSRT(JsGetTypedArrayInfo(typedArray, nullptr, nullptr, nullptr, &len));
  *result = static_cast<size_t>(len);
  return napi_ok;
}

napi_status napi_is_exception_pending(napi_env, bool* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsHasException(result));
  return napi_ok;
}

napi_status napi_get_and_clear_last_exception(napi_env e, napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsGetAndClearException(reinterpret_cast<JsValueRef*>(result)));
  if (*result == nullptr) {
    // Is this necessary?
    CHECK_NAPI(napi_get_undefined(e, result));
  }
  return napi_ok;
}
