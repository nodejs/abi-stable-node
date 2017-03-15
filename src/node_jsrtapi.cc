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

#include <stddef.h>
#include "node_jsvmapi_internal.h"
#include <node_buffer.h>
#include <node_object_wrap.h>
#include <env.h>
#include <vector>
#include <algorithm>
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
  napi_value thisArg;
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

namespace jsrtimpl {

  // Adapter for JSRT external data + finalize callback.
  class ExternalData {
  public:
    ExternalData(void* data, napi_finalize finalize_cb) : _data(data), _cb(finalize_cb) {
    }

    void* Data() {
      return _data;
    }

    // JsFinalizeCallback
    static void CALLBACK Finalize(void* callbackState) {
      ExternalData* externalData = reinterpret_cast<ExternalData*>(callbackState);
      if (externalData != nullptr) {
        if (externalData->_cb != nullptr) {
          externalData->_cb(externalData->_data);
        }

        delete externalData;
      }
    }

    // node::Buffer::FreeCallback
    static void Finalize(char* data, void* callbackState) {
      Finalize(callbackState);
    }

  private:
    void* _data;
    napi_finalize _cb;
  };

  // Adapter for JSRT external callback + callback data.
  class ExternalCallback {
  public:
    ExternalCallback(napi_callback cb, void* data) : _cb(cb), _data(data) {
    }

    // JsNativeFunction
    static JsValueRef CALLBACK Callback(JsValueRef callee,
                                        bool isConstructCall,
                                        JsValueRef *arguments,
                                        unsigned short argumentCount,
                                        void *callbackState) {
      jsrtimpl::ExternalCallback* externalCallback =
        reinterpret_cast<jsrtimpl::ExternalCallback*>(callbackState);

      JsErrorCode error = JsNoError;
      JsValueRef undefinedValue;
      error = JsGetUndefinedValue(&undefinedValue);

      CallbackInfo cbInfo;
      cbInfo.thisArg = reinterpret_cast<napi_value>(arguments[0]);
      cbInfo.isConstructCall = isConstructCall;
      cbInfo.argc = argumentCount - 1;
      cbInfo.argv = reinterpret_cast<napi_value*>(arguments + 1);
      cbInfo.data = externalCallback->_data;
      cbInfo.returnValue = reinterpret_cast<napi_value>(undefinedValue);

      if (isConstructCall) {
        // For constructor callbacks, replace the 'this' arg with a new external object,
        // to support wrapping a native object in the external object.
        JsValueRef externalThis;
        if (JsNoError == JsCreateExternalObject(
          nullptr, jsrtimpl::ExternalData::Finalize, &externalThis)) {

          // Copy the prototype from the default 'this' arg to the new 'external' this arg.
          if (arguments[0] != nullptr) {
            JsValueRef thisPrototype;
            if (JsNoError == JsGetPrototype(arguments[0], &thisPrototype)) {
              JsSetPrototype(externalThis, thisPrototype);
            }
          }

          cbInfo.thisArg = reinterpret_cast<napi_value>(externalThis);
          cbInfo.returnValue = cbInfo.thisArg;
        }
      }

      // TODO(tawoll): get environment pointer instead of nullptr?
      externalCallback->_cb(nullptr, reinterpret_cast<napi_callback_info>(&cbInfo));
      return reinterpret_cast<JsValueRef>(cbInfo.returnValue);
    }

    // JsObjectBeforeCollectCallback
    static void CALLBACK Finalize(JsRef ref, void* callbackState) {
      jsrtimpl::ExternalCallback* externalCallback =
        reinterpret_cast<jsrtimpl::ExternalCallback*>(callbackState);
      delete externalCallback;
    }

  private:
    napi_callback _cb;
    void* _data;
  };

}  // end of namespace jsrtimpl

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

#define CHECK_JSRT_EXPECTED(expr, expected)                             \
  do {                                                                  \
    JsErrorCode err = (expr);                                           \
    if (err == JsErrorInvalidArgument)                                  \
      return napi_set_last_error(expected);                             \
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
  "A number was expected",
  "A boolean was expected",
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
napi_status napi_get_current_env(napi_env* e) {
  *e = nullptr;
  return napi_ok;
}

napi_status napi_create_function(napi_env e, napi_callback cb, void* data, napi_value* result) {
  CHECK_ARG(result);

  jsrtimpl::ExternalCallback* externalCallback = new jsrtimpl::ExternalCallback(cb, data);
  if (externalCallback == nullptr) return napi_set_last_error(napi_generic_failure);

  JsValueRef function;
  CHECK_JSRT(JsCreateFunction(jsrtimpl::ExternalCallback::Callback, externalCallback, &function));

  CHECK_JSRT(JsSetObjectBeforeCollectCallback(
    function, externalCallback, jsrtimpl::ExternalCallback::Finalize));

  *result = reinterpret_cast<napi_value>(function);
  return napi_ok;
}

napi_status napi_define_class(napi_env e,
                              const char* utf8name,
                              napi_callback cb,
                              void* data,
                              int property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result) {
  CHECK_ARG(result);

  napi_value namestring;
  CHECK_NAPI(napi_create_string_utf8(e, utf8name, -1, &namestring));

  jsrtimpl::ExternalCallback* externalCallback = new jsrtimpl::ExternalCallback(cb, data);
  if (externalCallback == nullptr) return napi_set_last_error(napi_generic_failure);

  JsValueRef constructor;
  CHECK_JSRT(JsCreateNamedFunction(
    namestring, jsrtimpl::ExternalCallback::Callback, externalCallback, &constructor));

  CHECK_JSRT(JsSetObjectBeforeCollectCallback(
    constructor, externalCallback, jsrtimpl::ExternalCallback::Finalize));

  JsPropertyIdRef pid = nullptr;
  JsValueRef prototype = nullptr;
  CHECK_JSRT(JsCreatePropertyIdUtf8("prototype", 10, &pid));
  CHECK_JSRT(JsGetProperty(constructor, pid, &prototype));

  CHECK_JSRT(JsCreatePropertyIdUtf8("constructor", 12, &pid));
  CHECK_JSRT(JsSetProperty(prototype, pid, constructor, false));

  int instancePropertyCount = 0;
  int staticPropertyCount = 0;
  for (int i = 0; i < property_count; i++) {
    if ((properties[i].attributes & napi_static_property) != 0) {
      staticPropertyCount++;
    }
    else {
      instancePropertyCount++;
    }
  }

  std::vector<napi_property_descriptor> descriptors;
  descriptors.reserve(std::max(instancePropertyCount, staticPropertyCount));

  if (instancePropertyCount > 0) {
    for (int i = 0; i < property_count; i++) {
      if ((properties[i].attributes & napi_static_property) == 0) {
        descriptors.push_back(properties[i]);
      }
    }

    CHECK_NAPI(napi_define_properties(
      e, reinterpret_cast<napi_value>(prototype), descriptors.size(), descriptors.data()));
  }

  if (staticPropertyCount > 0) {
    descriptors.clear();
    for (int i = 0; i < property_count; i++) {
      if (!(properties[i].attributes & napi_static_property) != 0) {
        descriptors.push_back(properties[i]);
      }
    }

    CHECK_NAPI(napi_define_properties(
      e, reinterpret_cast<napi_value>(constructor), descriptors.size(), descriptors.data()));
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

napi_status napi_define_properties(napi_env e,
                                   napi_value o,
                                   int property_count,
                                   const napi_property_descriptor* properties) {
  JsPropertyIdRef configurableProperty;
  CHECK_JSRT(JsCreatePropertyIdUtf8("configurable", 12, &configurableProperty));

  JsPropertyIdRef enumerableProperty;
  CHECK_JSRT(JsCreatePropertyIdUtf8("enumerable", 10, &enumerableProperty));

  for (int i = 0; i < property_count; i++) {
    const napi_property_descriptor* p = properties + i;

    JsValueRef descriptor;
    CHECK_JSRT(JsCreateObject(&descriptor));

    JsValueRef configurable;
    CHECK_JSRT(JsBoolToBoolean(!(p->attributes & napi_dont_delete), &configurable));
    CHECK_JSRT(JsSetProperty(descriptor, configurableProperty, configurable, true));

    JsValueRef enumerable;
    CHECK_JSRT(JsBoolToBoolean(!(p->attributes & napi_dont_enum), &enumerable));
    CHECK_JSRT(JsSetProperty(descriptor, enumerableProperty, enumerable, true));

    if (p->method) {
      JsPropertyIdRef valueProperty;
      CHECK_JSRT(JsCreatePropertyIdUtf8("value", 5, &valueProperty));
      JsValueRef method;
      CHECK_NAPI(napi_create_function(e, p->method, p->data,
        reinterpret_cast<napi_value*>(&method)));
      CHECK_JSRT(JsSetProperty(descriptor, valueProperty, method, true));
    }
    else if (p->getter || p->setter) {
      if (p->getter) {
        JsPropertyIdRef getProperty;
        CHECK_JSRT(JsCreatePropertyIdUtf8("get", 3, &getProperty));
        JsValueRef getter;
        CHECK_NAPI(napi_create_function(e, p->getter, p->data,
          reinterpret_cast<napi_value*>(&getter)));
        CHECK_JSRT(JsSetProperty(descriptor, getProperty, getter, true));
      }

      if (p->setter) {
        JsPropertyIdRef setProperty;
        CHECK_JSRT(JsCreatePropertyIdUtf8("set", 5, &setProperty));
        JsValueRef setter;
        CHECK_NAPI(napi_create_function(e, p->setter, p->data,
          reinterpret_cast<napi_value*>(&setter)));
        CHECK_JSRT(JsSetProperty(descriptor, setProperty, setter, true));
      }
    }
    else {
      RETURN_STATUS_IF_FALSE(p->value != nullptr, napi_invalid_arg);

      JsPropertyIdRef writableProperty;
      CHECK_JSRT(JsCreatePropertyIdUtf8("writable", 8, &writableProperty));
      JsValueRef writable;
      CHECK_JSRT(JsBoolToBoolean(!(p->attributes & napi_read_only), &writable));
      CHECK_JSRT(JsSetProperty(descriptor, writableProperty, writable, true));

      JsPropertyIdRef valueProperty;
      CHECK_JSRT(JsCreatePropertyIdUtf8("value", 5, &valueProperty));
      CHECK_JSRT(JsSetProperty(descriptor, valueProperty,
        reinterpret_cast<JsValueRef>(p->value), true));
    }

    JsPropertyIdRef nameProperty;
    CHECK_JSRT(JsCreatePropertyIdUtf8(p->utf8name, strlen(p->utf8name), &nameProperty));
    bool result;
    CHECK_JSRT(JsDefineProperty(
      reinterpret_cast<JsValueRef>(o),
      reinterpret_cast<JsPropertyIdRef>(nameProperty),
      reinterpret_cast<JsValueRef>(descriptor),
      &result));
  }

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

napi_status napi_create_string_utf8(napi_env e, const char* s, int length,
                                    napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsCreateStringUtf8(
    reinterpret_cast<const uint8_t*>(s),
    length < 0 ? strlen(s) : static_cast<size_t>(length),
    reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_create_string_utf16(napi_env e, const char16_t* s, int length,
                                     napi_value* result) {
  CHECK_ARG(result);
  CHECK_JSRT(JsCreateStringUtf16(
    reinterpret_cast<const uint16_t*>(s),
    length < 0 ? std::char_traits<char16_t>::length(s) : static_cast<size_t>(length),
    reinterpret_cast<JsValueRef*>(result)));
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

napi_status napi_create_range_error(napi_env, napi_value msg, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef message = reinterpret_cast<JsValueRef>(msg);
  CHECK_JSRT(JsCreateRangeError(message, reinterpret_cast<JsValueRef*>(result)));
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

    default:
      // An "external" value is represented in JSRT as an Object that has external data and
      // DOES NOT allow extensions. (A wrapped object has external data and DOES allow extensions.)
      bool hasExternalData;
      if (JsHasExternalData(value, &hasExternalData) != JsNoError) {
        hasExternalData = false;
      }

      bool isExtensionAllowed;
      if (JsGetExtensionAllowed(value, &isExtensionAllowed) != JsNoError) {
        isExtensionAllowed = false;
      }

      *result = ((hasExternalData && !isExtensionAllowed) ? napi_external : napi_object);
      break;
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
  *result = info->argc;
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
  napi_value* buffer, int bufferlength) {
  CHECK_ARG(cbinfo);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);

  int i = 0;
  int min = std::min(bufferlength, static_cast<int>(info->argc));

  for (; i < min; i++) {
    buffer[i] = info->argv[i];
  }

  if (i < bufferlength) {
    napi_value undefined;
    CHECK_JSRT(JsGetUndefinedValue(reinterpret_cast<JsValueRef*>(&undefined)));
    for (; i < bufferlength; i += 1) {
      buffer[i] = undefined;
    }
  }

  return napi_ok;
}

napi_status napi_get_cb_this(napi_env e, napi_callback_info cbinfo, napi_value* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->thisArg;
  return napi_ok;
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_status napi_get_cb_holder(napi_env e, napi_callback_info cbinfo, napi_value* result) {
  CHECK_ARG(cbinfo);
  CHECK_ARG(result);
  const CallbackInfo *info = reinterpret_cast<CallbackInfo*>(cbinfo);
  *result = info->thisArg;
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

napi_status napi_throw_range_error(napi_env e, const char* msg) {
  JsValueRef strRef;
  JsValueRef exception;
  size_t length = strlen(msg);
  CHECK_JSRT(JsCreateStringUtf8((uint8_t*)msg, length, &strRef));
  CHECK_JSRT(JsCreateRangeError(strRef, &exception));
  CHECK_JSRT(JsSetException(exception));
  return napi_ok;
}

napi_status napi_get_value_double(napi_env e, napi_value v, double* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef numberValue = nullptr;
  double doubleValue = 0.0;
  CHECK_JSRT_EXPECTED(JsNumberToDouble(value, result), napi_number_expected);
  return napi_ok;
}

napi_status napi_get_value_int32(napi_env e, napi_value v, int32_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT_EXPECTED(JsNumberToInt(value, &valueInt), napi_number_expected);
  *result = static_cast<int32_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_uint32(napi_env e, napi_value v, uint32_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT_EXPECTED(JsNumberToInt(value, &valueInt), napi_number_expected);
  *result = static_cast<uint32_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_int64(napi_env e, napi_value v, int64_t* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  CHECK_JSRT_EXPECTED(JsNumberToInt(value, &valueInt), napi_number_expected);
  *result = static_cast<int64_t>(valueInt);
  return napi_ok;
}

napi_status napi_get_value_bool(napi_env e, napi_value v, bool* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef booleanValue = nullptr;
  CHECK_JSRT_EXPECTED(JsBooleanToBool(value, result), napi_boolean_expected);
  return napi_ok;
}

// Gets the number of CHARACTERS in the string.
napi_status napi_get_value_string_length(napi_env e, napi_value v, int* result) {
  CHECK_ARG(result);

  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT_EXPECTED(JsGetStringLength(value, result), napi_string_expected);
  return napi_ok;
}

// Gets the number of BYTES in the UTF-8 encoded representation of the string.
napi_status napi_get_value_string_utf8_length(napi_env e, napi_value v, int* result) {
  CHECK_ARG(result);

  JsValueRef value = reinterpret_cast<JsValueRef>(v);

  size_t length;
  CHECK_JSRT_EXPECTED(JsCopyStringUtf8(value, nullptr, 0, &length), napi_string_expected);
  *result = length;
  return napi_ok;
}

// Copies a JavaScript string into a UTF-8 string buffer. The result is the number
// of bytes copied into buf, including the null terminator. If the buf size is
// insufficient, the string will be truncated, including a null terminator.
napi_status napi_get_value_string_utf8(
    napi_env e, napi_value v, char* buf, int bufsize, int* result) {
  CHECK_ARG(buf);

  if (bufsize == 0) {
    *result = 0;
    return napi_ok;
  }

  JsValueRef value = reinterpret_cast<JsValueRef>(v);

  size_t copied = 0;
  CHECK_JSRT_EXPECTED(JsCopyStringUtf8(value, reinterpret_cast<uint8_t*>(buf),
    static_cast<size_t>(bufsize), &copied), napi_string_expected);

  if (copied < static_cast<size_t>(bufsize)) {
    buf[copied] = 0;
    copied++;
  }
  else {
    buf[bufsize - 1] = 0;
  }

  if (result != nullptr) {
    *result = static_cast<int>(copied);
  }
  return napi_ok;
}

// Gets the number of 2-byte code units in the UTF-16 encoded representation of the string.
napi_status napi_get_value_string_utf16_length(napi_env e, napi_value v, int* result) {
  CHECK_ARG(result);

  JsValueRef value = reinterpret_cast<JsValueRef>(v);

  size_t length;
  CHECK_JSRT_EXPECTED(JsCopyStringUtf16(value, 0, 0, nullptr, &length), napi_string_expected);
  *result = length;
  return napi_ok;
}

// Copies a JavaScript string into a UTF-16 string buffer. The result is the number
// of 2-byte code units copied into buf, including the null terminator. If the buf
// size is insufficient, the string will be truncated, including a null terminator.
napi_status napi_get_value_string_utf16(
    napi_env e, napi_value v, char16_t* buf, int bufsize, int* result) {
  CHECK_ARG(buf);

  if (bufsize == 0) {
    *result = 0;
    return napi_ok;
  }

  JsValueRef value = reinterpret_cast<JsValueRef>(v);

  size_t copied = 0;
  CHECK_JSRT_EXPECTED(JsCopyStringUtf16(value, 0, static_cast<size_t>(bufsize),
    reinterpret_cast<uint16_t*>(buf), &copied), napi_string_expected);

  if (copied < static_cast<size_t>(bufsize)) {
    buf[copied] = 0;
    copied++;
  }
  else {
    buf[bufsize - 1] = 0;
  }

  if (result != nullptr) {
    *result = static_cast<int>(copied);
  }
  return napi_ok;
}

napi_status napi_coerce_to_number(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsConvertValueToNumber(value, reinterpret_cast<JsValueRef*>(result)));
  return napi_ok;
}

napi_status napi_coerce_to_bool(napi_env e, napi_value v, napi_value* result) {
  CHECK_ARG(result);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  CHECK_JSRT(JsConvertValueToBoolean(value, reinterpret_cast<JsValueRef*>(result)));
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
    napi_finalize finalize_cb, napi_ref* result) {
  JsValueRef value = reinterpret_cast<JsValueRef>(jsObject);

  jsrtimpl::ExternalData* externalData = new jsrtimpl::ExternalData(nativeObj, finalize_cb);
  if (externalData == nullptr) return napi_set_last_error(napi_generic_failure);

  CHECK_JSRT(JsSetExternalData(value, externalData));

  if (result != nullptr) {
    napi_create_reference(e, jsObject, 0, result);
  }

  return napi_ok;
}

napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result) {
  return napi_get_value_external(e, jsObject, result);
}

napi_status napi_create_external(napi_env e, void* data, napi_finalize finalize_cb,
    napi_value* result) {
  CHECK_ARG(result);

  jsrtimpl::ExternalData* externalData = new jsrtimpl::ExternalData(data, finalize_cb);
  if (externalData == nullptr) return napi_set_last_error(napi_generic_failure);

  CHECK_JSRT(JsCreateExternalObject(
    externalData,
    jsrtimpl::ExternalData::Finalize,
    reinterpret_cast<JsValueRef*>(result)));
  CHECK_JSRT(JsPreventExtension(reinterpret_cast<JsValueRef*>(result)));

  return napi_ok;
}

napi_status napi_get_value_external(napi_env e, napi_value v, void** result) {
  CHECK_ARG(result);

  jsrtimpl::ExternalData* externalData;
  CHECK_JSRT(JsGetExternalData(
    reinterpret_cast<JsValueRef>(v),
    reinterpret_cast<void**>(&externalData)));

  *result = (externalData != nullptr ? externalData->Data() : nullptr);

  return napi_ok;
}

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
napi_status napi_create_reference(napi_env e, napi_value v, int initial_refcount,
    napi_ref* result) {
  CHECK_ARG(result);

  JsValueRef strongRef = reinterpret_cast<JsValueRef>(v);
  JsWeakRef weakRef = nullptr;
  CHECK_JSRT(JsCreateWeakReference(strongRef, &weakRef));

  // Prevent the reference itself from being collected until it is explicitly deleted.
  CHECK_JSRT(JsAddRef(weakRef, nullptr));

  // Apply the initial refcount to the target value.
  for (int i = 0; i < initial_refcount; i++) {
    CHECK_JSRT(JsAddRef(strongRef, nullptr));

    // Also increment the refcount of the reference by the same amount,
    // to enable reverting the target's refcount when the reference is deleted.
    CHECK_JSRT(JsAddRef(weakRef, nullptr));
  }

  *result = reinterpret_cast<napi_ref>(weakRef);
  return napi_ok;
}

// Deletes a reference. The referenced value is released, and may be GC'd unless there
// are other references to it.
napi_status napi_delete_reference(napi_env e, napi_ref ref) {
  JsRef weakRef = reinterpret_cast<JsRef>(ref);

  unsigned int count;
  CHECK_JSRT(JsRelease(weakRef, &count));

  if (count > 0) {
    // Revert this reference's contribution to the target's refcount.
    JsValueRef target;
    CHECK_JSRT(JsGetWeakReferenceValue(weakRef, &target));

    do {
      CHECK_JSRT(JsRelease(weakRef, &count));
      CHECK_JSRT(JsRelease(target, nullptr));
    } while (count > 0);
  }

  return napi_ok;
}

// Increments the reference count, optionally returning the resulting count. After this call the
// reference will be a strong reference because its refcount is >0, and the referenced object is
// effectively "pinned". Calling this when the refcount is 0 and the target is unavailable
// results in an error.
napi_status napi_reference_addref(napi_env e, napi_ref ref, int* result) {
  JsRef weakRef = reinterpret_cast<JsRef>(ref);

  JsValueRef target;
  CHECK_JSRT(JsGetWeakReferenceValue(weakRef, &target));

  if (target == nullptr) {
    // Called napi_reference_addref when the target is unavailable!
    return napi_set_last_error(napi_generic_failure);
  }

  CHECK_JSRT(JsAddRef(target, nullptr));

  unsigned int count;
  CHECK_JSRT(JsAddRef(weakRef, &count));

  if (result != nullptr) {
    *result = static_cast<int>(count - 1);
  }

  return napi_ok;
}

// Decrements the reference count, optionally returning the resulting count. If the result is
// 0 the reference is now weak and the object may be GC'd at any time if there are no other
// references. Calling this when the refcount is already 0 results in an error.
napi_status napi_reference_release(napi_env e, napi_ref ref, int* result) {
  JsRef weakRef = reinterpret_cast<JsRef>(ref);

  JsValueRef target;
  CHECK_JSRT(JsGetWeakReferenceValue(weakRef, &target));

  if (target == nullptr) {
    return napi_set_last_error(napi_generic_failure);
  }

  unsigned int count;
  CHECK_JSRT(JsRelease(weakRef, &count));
  if (count == 0) {
    // Called napi_release_reference too many times on a reference!
    return napi_set_last_error(napi_generic_failure);
  }

  CHECK_JSRT(JsRelease(target, nullptr));

  if (result != nullptr) {
    *result = static_cast<int>(count - 1);
  }

  return napi_ok;
}

// Attempts to get a referenced value. If the reference is weak, the value might no longer be
// available, in that case the call is still successful but the result is NULL.
napi_status napi_get_reference_value(napi_env e, napi_ref ref, napi_value* result) {
  CHECK_ARG(result);
  JsWeakRef weakRef = reinterpret_cast<JsWeakRef>(ref);
  CHECK_JSRT(JsGetWeakReferenceValue(weakRef, reinterpret_cast<JsValueRef*>(result)));
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

napi_status napi_create_buffer(napi_env e, size_t size, char** data, napi_value* result) {
  CHECK_ARG(result);
  // TODO(tawoll): Replace v8impl with jsrt-based version.

  v8::MaybeLocal<v8::Object> maybe = node::Buffer::New(v8impl::V8IsolateFromJsEnv(e), size);
  if (maybe.IsEmpty()) {
    return napi_generic_failure;
  }

  v8::Local<v8::Object> buffer = maybe.ToLocalChecked();
  if (data != nullptr) {
    *data = node::Buffer::Data(buffer);
  }

  *result = v8impl::JsValueFromV8LocalValue(buffer);
  return napi_ok;
}

napi_status napi_create_external_buffer(napi_env e,
                                        size_t size,
                                        char* data,
                                        napi_finalize finalize_cb,
                                        napi_value* result) {
  CHECK_ARG(result);
  // TODO(tawoll): Replace v8impl with jsrt-based version.

  jsrtimpl::ExternalData* externalData = new jsrtimpl::ExternalData(data, finalize_cb);
  v8::MaybeLocal<v8::Object> maybe = node::Buffer::New(
    v8impl::V8IsolateFromJsEnv(e),
    data,
    size,
    jsrtimpl::ExternalData::Finalize,
    externalData);

  if (maybe.IsEmpty()) {
    return napi_generic_failure;
  }

  *result = v8impl::JsValueFromV8LocalValue(maybe.ToLocalChecked());
  return napi_ok;
}

napi_status napi_create_buffer_copy(napi_env e,
                                    const char* data,
                                    size_t size,
                                    napi_value* result) {
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

napi_status napi_is_buffer(napi_env e, napi_value v, bool* result) {
  CHECK_ARG(result);
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsValueType objectType;
  CHECK_JSRT(JsGetValueType(typedArray, &objectType));
  if (objectType != JsTypedArray) {
    *result = false;
    return napi_ok;
  }
  JsTypedArrayType arrayType;
  CHECK_JSRT(JsGetTypedArrayInfo(typedArray, &arrayType, nullptr, nullptr, nullptr));
  *result = (arrayType == JsArrayTypeUint8);
  return napi_ok;
}

napi_status napi_get_buffer_info(napi_env e, napi_value v, char** data, size_t* length) {
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsValueRef arrayBuffer;
  unsigned int byteOffset;
  ChakraBytePtr buffer;
  unsigned int bufferLength;
  CHECK_JSRT(JsGetTypedArrayInfo(typedArray, nullptr, &arrayBuffer, &byteOffset, nullptr));
  CHECK_JSRT(JsGetArrayBufferStorage(arrayBuffer, &buffer, &bufferLength));

  if (data != nullptr) {
    *data = reinterpret_cast<char*>(buffer + byteOffset);
  }

  if (length != nullptr) {
    *length = static_cast<size_t>(bufferLength);
  }

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

napi_status napi_is_arraybuffer(napi_env e, napi_value value, bool* result) {
  CHECK_ARG(result);

  JsValueRef jsValue = reinterpret_cast<JsValueRef>(value);
  JsValueType valueType;
  CHECK_JSRT(JsGetValueType(jsValue, &valueType));

  *result = (valueType == JsArrayBuffer);
  return napi_ok;
}

napi_status napi_create_arraybuffer(napi_env e,
                                    size_t byte_length,
                                    void** data,
                                    napi_value* result) {
  CHECK_ARG(result);

  JsValueRef arrayBuffer;
  CHECK_JSRT(JsCreateArrayBuffer(static_cast<unsigned int>(byte_length), &arrayBuffer));

  if (data != nullptr) {
    CHECK_JSRT(JsGetArrayBufferStorage(
      arrayBuffer,
      reinterpret_cast<ChakraBytePtr*>(data),
      reinterpret_cast<unsigned int*>(&byte_length)));
  }

  *result = reinterpret_cast<napi_value>(arrayBuffer);
  return napi_ok;
}

napi_status napi_create_external_arraybuffer(napi_env e,
                                             void* external_data,
                                             size_t byte_length,
                                             napi_finalize finalize_cb,
                                             napi_value* result) {
  CHECK_ARG(result);

  jsrtimpl::ExternalData* externalData = new jsrtimpl::ExternalData(external_data, finalize_cb);
  if (externalData == nullptr) return napi_set_last_error(napi_generic_failure);

  JsValueRef arrayBuffer;
  CHECK_JSRT(JsCreateExternalArrayBuffer(
    external_data,
    static_cast<unsigned int>(byte_length),
    jsrtimpl::ExternalData::Finalize,
    externalData,
    &arrayBuffer));

  *result = reinterpret_cast<napi_value>(arrayBuffer);
  return napi_ok;
}

napi_status napi_get_arraybuffer_info(napi_env e,
                                      napi_value arraybuffer,
                                      void** data,
                                      size_t* byte_length) {
  ChakraBytePtr storageData;
  unsigned int storageLength;
  CHECK_JSRT(JsGetArrayBufferStorage(
    reinterpret_cast<JsValueRef>(arraybuffer),
    &storageData,
    &storageLength));

  if (data != nullptr) {
    *data = reinterpret_cast<void*>(storageData);
  }

  if (byte_length != nullptr) {
    *byte_length = static_cast<size_t>(storageLength);
  }

  return napi_ok;
}

napi_status napi_is_typedarray(napi_env e, napi_value value, bool* result) {
  CHECK_ARG(result);

  JsValueRef jsValue = reinterpret_cast<JsValueRef>(value);
  JsValueType valueType;
  CHECK_JSRT(JsGetValueType(jsValue, &valueType));

  *result = (valueType == JsTypedArray);
  return napi_ok;
}

napi_status napi_create_typedarray(napi_env e,
                                   napi_typedarray_type type,
                                   size_t length,
                                   napi_value arraybuffer,
                                   size_t byte_offset,
                                   napi_value* result) {
  CHECK_ARG(result);

  JsTypedArrayType jsType;
  switch (type) {
    case napi_int8:
      jsType = JsArrayTypeInt8;
      break;
    case napi_uint8:
      jsType = JsArrayTypeUint8;
      break;
    case napi_uint8_clamped:
      jsType = JsArrayTypeUint8Clamped;
      break;
    case napi_int16:
      jsType = JsArrayTypeInt16;
      break;
    case napi_uint16:
      jsType = JsArrayTypeUint16;
      break;
    case napi_int32:
      jsType = JsArrayTypeInt32;
      break;
    case napi_uint32:
      jsType = JsArrayTypeUint32;
      break;
    case napi_float32:
      jsType = JsArrayTypeFloat32;
      break;
    case napi_float64:
      jsType = JsArrayTypeFloat64;
      break;
    default:
      return napi_set_last_error(napi_invalid_arg);
  }

  JsValueRef jsArrayBuffer = reinterpret_cast<JsValueRef>(arraybuffer);

  CHECK_JSRT(JsCreateTypedArray(
    jsType,
    jsArrayBuffer,
    static_cast<unsigned int>(byte_offset),
    static_cast<unsigned int>(length),
    reinterpret_cast<JsValueRef*>(result)));

  return napi_ok;
}

napi_status napi_get_typedarray_info(napi_env e,
                                     napi_value typedarray,
                                     napi_typedarray_type* type,
                                     size_t* length,
                                     void** data,
                                     napi_value* arraybuffer,
                                     size_t* byte_offset) {
  JsTypedArrayType jsType;
  JsValueRef jsArrayBuffer;
  unsigned int byteOffset;
  unsigned int byteLength;
  ChakraBytePtr bufferData;
  unsigned int bufferLength;
  int elementSize;

  CHECK_JSRT(JsGetTypedArrayInfo(
    reinterpret_cast<JsValueRef>(typedarray),
    &jsType,
    &jsArrayBuffer,
    &byteOffset,
    &byteLength));

  CHECK_JSRT(JsGetTypedArrayStorage(
    reinterpret_cast<JsValueRef>(typedarray),
    &bufferData,
    &bufferLength,
    &jsType,
    &elementSize));

  if (type != nullptr) {
    switch (jsType) {
      case JsArrayTypeInt8:
        *type = napi_int8;
        break;
      case JsArrayTypeUint8:
        *type = napi_uint8;
        break;
      case JsArrayTypeUint8Clamped:
        *type = napi_uint8_clamped;
        break;
      case JsArrayTypeInt16:
        *type = napi_int16;
        break;
      case JsArrayTypeUint16:
        *type = napi_uint16;
        break;
      case JsArrayTypeInt32:
        *type = napi_int32;
        break;
      case JsArrayTypeUint32:
        *type = napi_uint32;
        break;
      case JsArrayTypeFloat32:
        *type = napi_float32;
        break;
      case JsArrayTypeFloat64:
        *type = napi_float64;
        break;
      default:
        return napi_set_last_error(napi_generic_failure);
    }
  }

  if (length != nullptr) {
    *length = static_cast<size_t>(byteLength / elementSize);
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t*>(bufferData) + byteOffset;
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = reinterpret_cast<napi_value>(jsArrayBuffer);
  }

  if (byte_offset != nullptr) {
    *byte_offset = static_cast<size_t>(byteOffset);
  }

  return napi_ok;
}
