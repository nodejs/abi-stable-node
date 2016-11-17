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

#include "src\jsrtutils.h"

#ifndef CALLBACK
#define CALLBACK  
#endif

typedef void napi_destruct(void* v);

//Callback Info struct as per JSRT native function.
typedef struct CallbackInfo {
  JsValueRef _callee;
  bool _isConstructCall;
  JsValueRef *_arguments;
  unsigned short _argumentCount;
  napi_value returnValue;
}CbInfo;

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
    return (napi_persistent)per;
  };

  static v8::Persistent<v8::Value>* V8PersistentValueFromJsPersistentValue(
    napi_persistent per) {
    return (v8::Persistent<v8::Value>*) per;
  }

	static napi_weakref JsWeakRefFromV8PersistentValue(v8::Persistent<v8::Value> *per) {
		return (napi_weakref) per;
	}

	static v8::Persistent<v8::Value>* V8PersistentValueFromJsWeakRefValue(napi_weakref per) {
		return (v8::Persistent<v8::Value>*) per;
	}

	static void WeakRefCallback(const v8::WeakCallbackInfo<int>& data) {
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

  static void FunctionCallbackWrapper(
    const v8::FunctionCallbackInfo<v8::Value> &info) {
    napi_callback cb = reinterpret_cast<napi_callback>(
      info.Data().As<v8::External>()->Value());
    JsErrorCode error = JsNoError;
    JsValueRef undefinedValue;
    error = JsGetUndefinedValue(&undefinedValue);
    CallbackInfo cbInfo;
    cbInfo._callee = reinterpret_cast<JsValueRef>(JsValueFromV8LocalValue(info.Callee()));
    cbInfo._argumentCount = info.Length() + 1;
    cbInfo._isConstructCall = info.IsConstructCall();
    cbInfo.returnValue = reinterpret_cast<napi_value>(undefinedValue);
    std::vector<JsValueRef> buffer(cbInfo._argumentCount);
    buffer[0] = reinterpret_cast<JsValueRef>(JsValueFromV8LocalValue(info.This()));
    for (int i = 0; i < cbInfo._argumentCount - 1; i++) {
      buffer[i + 1] = reinterpret_cast<JsValueRef>(JsValueFromV8LocalValue(info[i]));
    }
    cbInfo._arguments = buffer.data();
    cb(nullptr, reinterpret_cast<napi_func_cb_info>(&cbInfo));
    info.GetReturnValue().Set(V8LocalValueFromJsValue(cbInfo.returnValue));
  }

}  // end of namespace v8impl

void CHAKRA_CALLBACK JsObjectWrapWrapperBeforeCollectCallback(JsRef ref, void* callbackState);

class ObjectWrapWrapper : public node::ObjectWrap {
public:
  ObjectWrapWrapper(napi_value jsObject, void* nativeObj, napi_destruct* destructor) {
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
  napi_destruct* _destructor;
  void* _nativeObj;
};

void CHAKRA_CALLBACK JsObjectWrapWrapperBeforeCollectCallback(JsRef ref, void* callbackState)
{
  ObjectWrapWrapper* wrapper = reinterpret_cast<ObjectWrapWrapper*>(callbackState);
  delete wrapper;
}

//Stub for now
napi_env napi_get_current_env() {
  return nullptr;
}

_Ret_maybenull_ JsValueRef CALLBACK CallbackWrapper(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState) {
  JsErrorCode error = JsNoError;
  JsValueRef undefinedValue;
  error = JsGetUndefinedValue(&undefinedValue);
  CbInfo cbInfo;
  cbInfo._callee = callee;
  cbInfo._isConstructCall = isConstructCall;
  cbInfo._argumentCount = argumentCount;
  cbInfo._arguments = arguments;
  cbInfo.returnValue = reinterpret_cast<napi_value>(undefinedValue);
  napi_callback cb = reinterpret_cast<napi_callback>(callbackState);
  // TODO(tawoll): get environment pointer instead of nullptr?
  cb(nullptr, reinterpret_cast<napi_func_cb_info>(&cbInfo));
  return reinterpret_cast<JsValueRef>(cbInfo.returnValue);
}

napi_value napi_create_function(napi_env e, napi_callback cb) {
  JsErrorCode error;
  JsValueRef function;
  error = JsCreateFunction(CallbackWrapper, (void*)cb, &function);
  return reinterpret_cast<napi_value>(function);
}

napi_value napi_create_constructor_for_wrap(napi_env e, napi_callback cb) {
  // TODO(tawoll): evaluate if we need to do something special here (I don't think so)
  return napi_create_function(e, cb);
}

napi_value napi_create_constructor_for_wrap_with_methods(
  napi_env e,
  napi_callback cb,
  char* utf8name,
  int methodcount,
  napi_method_descriptor* methods) {

  napi_value namestring = napi_create_string(e, utf8name);
  JsValueRef constructor = nullptr;
  JsCreateNamedFunction(namestring, CallbackWrapper, (void*)cb, &constructor);

  JsPropertyIdRef pid = nullptr;
  JsValueRef prototype = nullptr;
  JsGetPropertyIdFromNameUtf8("prototype", &pid);
  //JsGetPrototype(constructor, &prototype);
  JsGetProperty(constructor, pid, &prototype);

  JsGetPropertyIdFromNameUtf8("constructor", &pid);
  JsSetProperty(prototype, pid, constructor, false);

  for (int i = 0; i < methodcount; i++) {
    namestring = napi_create_string(e, methods[i].utf8name);
    JsValueRef function = nullptr;
    JsCreateNamedFunction(namestring, CallbackWrapper, (void*)methods[i].callback, &function);
    //JsCreateFunction(CallbackWrapper, methods[i].callback, &function);

    JsGetPropertyIdFromNameUtf8(methods[i].utf8name, &pid);
    // TODO(tawoll): always use strict rules?
    JsSetProperty(prototype, pid, function, false);
    //JsSetProperty(constructor, pid, function, true);
  }

  return reinterpret_cast<napi_value>(constructor);
}

//Need to re-look as to create property descriptor correctly or not, cached property thing!!!
void napi_set_function_name(napi_env e, napi_value func,
  napi_propertyname name) {
  JsErrorCode error = JsNoError;
  JsValueRef object = reinterpret_cast<JsValueRef>(func);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(name);
  bool result;
  error = JsDefineProperty(object, propertyId, nullptr, &result);
}

void napi_set_return_value(napi_env e,
  napi_func_cb_info cbinfo, napi_value v) {
  CallbackInfo *info = (CallbackInfo*)cbinfo;
  info->returnValue = v;
}

napi_propertyname napi_property_name(napi_env e, const char* utf8name) {
  JsErrorCode error = JsNoError;
  JsPropertyIdRef propertyId = nullptr;
  error = JsGetPropertyIdFromNameUtf8(utf8name, &propertyId);
  return reinterpret_cast<napi_propertyname>(propertyId);
}

napi_value napi_get_propertynames(napi_env e, napi_value o) {
  JsErrorCode error = JsNoError;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef propertyNames = nullptr;
  error = JsGetOwnPropertyNames(object, &propertyNames);
  return reinterpret_cast<napi_value>(propertyNames);
}

void napi_set_property(napi_env e, napi_value o,
  napi_propertyname k, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  error = JsSetProperty(object, propertyId, value, true);

  // This implementation is missing a lot of details, notably error
  // handling on invalid inputs and regarding what happens in the
  // Set operation (error thrown, key is invalid, the bool return
  // value of Set)
}

bool napi_has_property(napi_env e, napi_value o, napi_propertyname k) {
  JsErrorCode error = JsNoError;
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  bool hasProperty = false;
  error = JsHasProperty(object, propertyId, &hasProperty);
  return hasProperty;
}

napi_value napi_get_property(napi_env e, napi_value o, napi_propertyname k) {
  JsErrorCode error = JsNoError;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsPropertyIdRef propertyId = reinterpret_cast<JsPropertyIdRef>(k);
  JsValueRef value = nullptr;
  error = JsGetProperty(object, propertyId, &value);
  return reinterpret_cast<napi_value>(value);
}

void napi_set_element(napi_env e, napi_value o, uint32_t i, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef index = nullptr;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  error = JsIntToNumber(i, &index);
  error = JsSetIndexedProperty(object, index, value);
}

bool napi_has_element(napi_env e, napi_value o, uint32_t i) {
  JsErrorCode error = JsNoError;
  JsValueRef index = nullptr;
  bool result = false;
  error = JsIntToNumber(i, &index);
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  error = JsHasIndexedProperty(object, index, &result);
  return result;
}

napi_value napi_get_element(napi_env e, napi_value o, uint32_t i) {
  JsErrorCode error = JsNoError;
  JsValueRef index = nullptr;
  JsValueRef result = nullptr;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  error = JsIntToNumber(i, &index);
  error = JsGetIndexedProperty(object, index, &result);
  return reinterpret_cast<napi_value>(result);
}

bool napi_is_array(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueType type = JsUndefined;
  error = JsGetValueType(value, &type);
  return (type == JsArray);
}

uint32_t napi_get_array_length(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsPropertyIdRef propertyIdRef;
  error = JsGetPropertyIdFromNameUtf8("length", &propertyIdRef);
  JsValueRef lengthRef;
  JsValueRef arrayRef = reinterpret_cast<JsValueRef>(v);
  error = JsGetProperty(arrayRef, propertyIdRef, &lengthRef);
  double sizeInDouble;
  error = JsNumberToDouble(lengthRef, &sizeInDouble);
  return static_cast<unsigned int>(sizeInDouble);
}

bool napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs) {
  JsErrorCode error = JsNoError;
  JsValueRef object1 = reinterpret_cast<JsValueRef>(lhs);
  JsValueRef object2 = reinterpret_cast<JsValueRef>(rhs);
  bool result = false;
  error = JsStrictEquals(object1, object2, &result);
  return result;
}

napi_value napi_get_prototype(napi_env e, napi_value o) {
  JsErrorCode error = JsNoError;
  JsValueRef object = reinterpret_cast<JsValueRef>(o);
  JsValueRef prototypeObject = nullptr;
  error = JsGetPrototype(object, &prototypeObject);
  return reinterpret_cast<napi_value>(prototypeObject);
}

napi_value napi_create_object(napi_env e) {
  JsErrorCode error = JsNoError;
  JsValueRef object = nullptr;
  error = JsCreateObject(&object);
  return reinterpret_cast<napi_value>(object);
}

napi_value napi_create_array(napi_env e) {
  JsErrorCode error = JsNoError;
  JsValueRef result = nullptr;
  unsigned int length = 0;
  error = JsCreateArray(length, &result);
  return reinterpret_cast<napi_value>(result);
}

napi_value napi_create_array_with_length(napi_env e, int length) {
  JsErrorCode error = JsNoError;
  JsValueRef result = nullptr;
  error = JsCreateArray(length, &result);
  return reinterpret_cast<napi_value>(result);
}

napi_value napi_create_string(napi_env e, const char* s) {
  JsErrorCode error = JsNoError;
  size_t length = strlen(s);
  JsValueRef strRef = nullptr;
  error = JsCreateStringUtf8((uint8_t*)s, length, &strRef);
  return reinterpret_cast<napi_value>(strRef);
}

napi_value napi_create_string_with_length(napi_env e,
  const char* s, size_t length) {
  JsErrorCode error = JsNoError;
  JsValueRef strRef = nullptr;
  error = JsCreateStringUtf8((uint8_t*)s, length, &strRef);
  return reinterpret_cast<napi_value>(strRef);
}

napi_value napi_create_number(napi_env e, double v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = nullptr;
  error = JsDoubleToNumber(v, &value);
  return reinterpret_cast<napi_value>(value);
}

napi_value napi_create_boolean(napi_env e, bool b) {
  JsErrorCode error = JsNoError;
  JsValueRef booleanValue = nullptr;
  error = JsDoubleToNumber(b, &booleanValue);
  return reinterpret_cast<napi_value>(booleanValue);
}

napi_value napi_create_error(napi_env, napi_value msg) {
  JsErrorCode error = JsNoError;
  JsValueRef message = reinterpret_cast<JsValueRef>(msg);
  JsValueRef errorMsg = nullptr;
  error = JsCreateError(message, &errorMsg);
  return reinterpret_cast<napi_value>(errorMsg);
}

napi_value napi_create_type_error(napi_env, napi_value msg) {
  JsErrorCode error = JsNoError;
  JsValueRef message = reinterpret_cast<JsValueRef>(msg);
  JsValueRef errorMsg = nullptr;
  error = JsCreateTypeError(message, &errorMsg);
  return reinterpret_cast<napi_value>(errorMsg);
}

napi_valuetype napi_get_type_of_value(napi_env e, napi_value vv) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(vv);
  JsValueType valueType = JsUndefined;
  error = JsGetValueType(value, &valueType);

  if (valueType == JsNumber) {
    return napi_number;
  }
  else if (valueType == JsString) {
    return napi_string;
  }
  else if (valueType == JsFunction) {
    // This test has to come before IsObject because IsFunction
    // implies IsObject
    return napi_function;
  }
  else if (valueType == JsObject) {
    return napi_object;
  }
  else if (valueType == JsBoolean) {
    return napi_boolean;
  }
  else if (valueType == JsUndefined) {
    return napi_undefined;
  } /*else if (v->IsSymbol()) {
    return napi_symbol;
    }*/ else if (valueType == JsNull) {
    return napi_null;
  }
    else {
      return napi_object;   // Is this correct?
    }
}

napi_value napi_get_undefined_(napi_env e) {
  JsValueRef undefinedValue = nullptr;
  JsErrorCode error = JsNoError;
  error = JsGetUndefinedValue(&undefinedValue);
  return reinterpret_cast<napi_value>(undefinedValue);
}

napi_value napi_get_null(napi_env e) {
  JsValueRef nullValue = nullptr;
  JsErrorCode error = JsNoError;
  error = JsGetNullValue(&nullValue);
  return reinterpret_cast<napi_value>(nullValue);
}

napi_value napi_get_false(napi_env e) {
  JsValueRef falseValue = nullptr;
  JsErrorCode error = JsNoError;
  error = JsGetFalseValue(&falseValue);
  return reinterpret_cast<napi_value>(falseValue);
}

napi_value napi_get_true(napi_env e) {
  JsValueRef trueValue = nullptr;
  JsErrorCode error = JsNoError;
  error = JsGetTrueValue(&trueValue);
  return reinterpret_cast<napi_value>(trueValue);
}

int napi_get_cb_args_length(napi_env e, napi_func_cb_info cbinfo) {
  const CallbackInfo *info = (CallbackInfo*)cbinfo;
  return (info->_argumentCount) - 1;
}

bool napi_is_construct_call(napi_env e, napi_func_cb_info cbinfo) {
  const CallbackInfo *info = (CallbackInfo*)cbinfo;
  return info->_isConstructCall;
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
void napi_get_cb_args(napi_env e, napi_func_cb_info cbinfo,
  napi_value* buffer, size_t bufferlength) {
  const CallbackInfo *info = (CallbackInfo*)cbinfo;

  int i = 0;
  // size_t appropriate for the buffer length parameter?
  // Probably this API is not the way to go.
  int min =
    static_cast<int>(bufferlength) < (info->_argumentCount) - 1 ?
    static_cast<int>(bufferlength) : (info->_argumentCount) - 1;

  for (; i < min; i++) {
    buffer[i] = reinterpret_cast<napi_value>(info->_arguments[i + 1]);
  }

  if (i < static_cast<int>(bufferlength)) {
    JsErrorCode error = JsNoError;
    JsValueRef undefinedValue;
    error = JsGetUndefinedValue(&undefinedValue);
    napi_value undefined = reinterpret_cast<napi_value>(undefinedValue);
    for (; i < static_cast<int>(bufferlength); i += 1) {
      buffer[i] = undefined;
    }
  }
}

napi_value napi_get_cb_this(napi_env e, napi_func_cb_info cbinfo) {
  const CallbackInfo *info = (CallbackInfo*)(cbinfo);
  return reinterpret_cast<napi_value>(info->_arguments[0]);
}

// Holder is a V8 concept.  Is not clear if this can be emulated with other VMs
// AFAIK Holder should be the owner of the JS function, which should be in the
// prototype chain of This, so maybe it is possible to emulate.
napi_value napi_get_cb_holder(napi_env e, napi_func_cb_info cbinfo) {
	const CallbackInfo *info = (CallbackInfo*)(cbinfo);
	return reinterpret_cast<napi_value>(info->_arguments[0]);
}

napi_value napi_call_function(napi_env e, napi_value scope,
  napi_value func, int argc, napi_value* argv) {
  JsValueRef object = reinterpret_cast<JsValueRef>(scope);
  JsValueRef function = reinterpret_cast<JsValueRef>(func);
  JsErrorCode error = JsNoError;
  std::vector<JsValueRef> args(argc+1);
  args[0] = object;
  for (int i = 0; i < argc; i++) {
    args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
  }
  JsValueRef result;
  error = JsCallFunction(function, args.data(), argc + 1, &result);
  return reinterpret_cast<napi_value>(result);
}

napi_value napi_get_global_scope(napi_env e) {
  JsValueRef globalObject = nullptr;
  JsErrorCode error = JsNoError;
  error = JsGetGlobalObject(&globalObject);
  return reinterpret_cast<napi_value>(globalObject);
}

void napi_throw(napi_env e, napi_value error) {
  JsErrorCode errorCode = JsNoError;
  JsValueRef exception = reinterpret_cast<JsValueRef>(error);
  errorCode = JsSetException(exception);
}

void napi_throw_error(napi_env e, char* msg) {
  JsErrorCode error = JsNoError;
  JsValueRef strRef;
  JsValueRef exception;
  size_t length = strlen(msg);
  error = JsCreateStringUtf8((uint8_t*)msg, length, &strRef);
  error = JsCreateError(strRef, &exception);
  error = JsSetException(exception);
}

void napi_throw_type_error(napi_env e, char* msg) {
  JsErrorCode error = JsNoError;
  JsValueRef strRef;
  JsValueRef exception;
  size_t length = strlen(msg);
  error = JsCreateStringUtf8((uint8_t*)msg, length, &strRef);
  error = JsCreateTypeError(strRef, &exception);
  error = JsSetException(exception);
}

double napi_get_number_from_value(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef numberValue = nullptr;
  double doubleValue = 0.0;
  error = JsConvertValueToNumber(value, &numberValue);
  error = JsNumberToDouble(numberValue, &doubleValue);
  return doubleValue;
}

int napi_get_string_from_value(napi_env e, napi_value v,
  char* buf, const int buf_size) {
  JsErrorCode error = JsNoError;
  int len = 0;
  size_t copied = 0;
  JsValueRef stringValue = reinterpret_cast<JsValueRef>(v);
  error = JsGetStringLength(stringValue, &len);
  error = JsWriteStringUtf8(stringValue, (uint8_t *)buf, buf_size, &copied);
  // add one for null ending
  return len - static_cast<int>(copied) + 1;
}

int32_t napi_get_value_int32(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  error = JsConvertValueToNumber(value, &numberValue);
  error = JsNumberToInt(numberValue, &valueInt);
  return static_cast<int32_t>(valueInt);
}

uint32_t napi_get_value_uint32(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  error = JsConvertValueToNumber(value, &numberValue);
  error = JsNumberToInt(numberValue, &valueInt);
  return static_cast<uint32_t>(valueInt);
}

int64_t napi_get_value_int64(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  int valueInt;
  JsValueRef numberValue = nullptr;
  error = JsConvertValueToNumber(value, &numberValue);
  error = JsNumberToInt(numberValue, &valueInt);
  return static_cast<int64_t>(valueInt);
}

bool napi_get_value_bool(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef booleanValue = nullptr;
  bool boolValue = false;
  error = JsConvertValueToBoolean(value, &booleanValue);
  error = JsBooleanToBool(booleanValue, &boolValue);
  return boolValue;
}

int napi_get_string_length(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef stringValue = reinterpret_cast<JsValueRef>(v);
  int length = 0;
  error = JsGetStringLength(stringValue, &length);
  return length;
}

int napi_get_string_utf8_length(napi_env e, napi_value v) {
  JsValueRef strRef = reinterpret_cast<JsValueRef>(v);
  size_t len = 0;
  int length = 0;
  char* str;
  JsErrorCode errorCode = JsStringToPointerUtf8Copy(strRef, &str, &len);
  if (errorCode == JsNoError) {
    length = static_cast<int>(len);
  }
  return length;
}

int napi_get_string_utf8(napi_env e, napi_value v, char* buf, int bufsize) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  size_t len = 0;
  error = JsWriteStringUtf8(value, (uint8_t*)buf, bufsize, &len);
  return static_cast<int>(len);
}

napi_value napi_coerce_to_object(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef object;
  error = JsConvertValueToObject(value, &object);
  return reinterpret_cast<napi_value>(object);
}

napi_value napi_coerce_to_string(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef value = reinterpret_cast<JsValueRef>(v);
  JsValueRef stringValue = nullptr;
  error = JsConvertValueToString(value, &stringValue);
  return reinterpret_cast<napi_value>(stringValue);
}

void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
  napi_destruct* destructor, napi_weakref* handle) {
  new ObjectWrapWrapper(jsObject, nativeObj, destructor);

  if (handle != nullptr)
  {
    *handle = napi_create_weakref(e, jsObject);
  }
}

void* napi_unwrap(napi_env e, napi_value jsObject) {
  return ObjectWrapWrapper::Unwrap(jsObject);
}

napi_persistent napi_create_persistent(napi_env e, napi_value v) {
    JsErrorCode error = JsNoError;
    JsValueRef value = reinterpret_cast<JsValueRef>(v);
    if (value) {
        error = JsAddRef(static_cast<JsRef>(value), nullptr);
    }
    return reinterpret_cast<napi_persistent>(value);
}

void napi_release_persistent(napi_env e, napi_persistent p) {
    JsErrorCode error = JsNoError;
    JsValueRef thePersistent = reinterpret_cast<JsValueRef>(p);
    error = JsRelease(static_cast<JsRef>(thePersistent), nullptr);
    thePersistent = nullptr;
}

napi_value napi_get_persistent_value(napi_env e, napi_persistent p) {
    JsValueRef value = reinterpret_cast<JsValueRef>(p);
    return reinterpret_cast<napi_value>(value);
}

napi_weakref napi_create_weakref(napi_env e, napi_value v)
{  
  //JsErrorCode error = JsNoError;
  JsValueRef strongRef = reinterpret_cast<JsValueRef>(v);
  JsWeakRef weakRef = nullptr;

  JsCreateWeakReference(strongRef, &weakRef);
  JsAddRef(weakRef, nullptr);

  return reinterpret_cast<napi_weakref>(weakRef);
}

bool napi_get_weakref_value(napi_env e, napi_weakref w, napi_value* pv)
{
    //JsErrorCode error = JsNoError;
    JsWeakRef weakRef = reinterpret_cast<JsWeakRef>(w);
    JsValueRef value = nullptr;

    JsGetWeakReferenceValue(weakRef, &value);
    if (value == nullptr)
    {
        *pv = nullptr;
        return false;
    }
    *pv = reinterpret_cast<napi_value>(value);
    return true;
}

void napi_release_weakref(napi_env e, napi_weakref w) {
  JsRef weakRef = reinterpret_cast<JsRef>(w);
  JsRelease(weakRef, nullptr);
}

/*********Stub implementation of handle scope apis' for JSRT***********/
napi_handle_scope napi_open_handle_scope(napi_env) {
  return nullptr;
}

void napi_close_handle_scope(napi_env, napi_handle_scope) {}

napi_escapable_handle_scope napi_open_escapable_handle_scope(napi_env) {
  return nullptr;
}

void napi_close_escapable_handle_scope(napi_env, napi_escapable_handle_scope) {}

//This one will return escapee value as this is called from leveldown db.
napi_value napi_escape_handle(napi_env e, napi_escapable_handle_scope scope,
	napi_value escapee) {
  return escapee;
}
/**************************************************************/

napi_value napi_new_instance(napi_env e, napi_value cons,
  int argc, napi_value *argv) {
  JsValueRef function = reinterpret_cast<JsValueRef>(cons);
  JsErrorCode error = JsNoError;
  JsValueRef undefinedValue;
  JsValueRef result;
  std::vector<JsValueRef> args(argc + 1);
  error = JsGetUndefinedValue(&undefinedValue);
  args[0] = undefinedValue;
	result = undefinedValue;
  if (argc > 0) {
    for (int i = 0; i < argc; i++) {
      args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
    }
  }
  error = JsConstructObject(function, args.data(), argc + 1, &result);
  return reinterpret_cast<napi_value>(result);
}

napi_value napi_make_external(napi_env e, napi_value v) {
  JsValueRef externalObj = nullptr;
  JsCreateExternalObject(NULL, NULL, &externalObj);
  JsSetPrototype(externalObj, reinterpret_cast<JsValueRef>(v));
  return reinterpret_cast<napi_value>(externalObj);
}

napi_value napi_make_callback(napi_env e, napi_value recv,
  napi_value func, int argc, napi_value* argv) {
	JsValueRef object = reinterpret_cast<JsValueRef>(recv);
	JsValueRef function = reinterpret_cast<JsValueRef>(func);
	JsErrorCode error = JsNoError;
	std::vector<JsValueRef> args(argc + 1);
	args[0] = object;
	for (int i = 0; i < argc; i++)
	{
		args[i + 1] = reinterpret_cast<JsValueRef>(argv[i]);
	}
	JsValueRef result;
	error = JsCallFunction(function, args.data(), argc + 1, &result);
	return reinterpret_cast<napi_value>(result);
}

bool napi_try_catch(napi_env e, napi_try_callback cbtry,
  napi_catch_callback cbcatch, void* data) {
  JsValueRef error = JS_INVALID_REFERENCE;
  bool hasException;
  cbtry(e, data);
  if (error = JS_INVALID_REFERENCE) {
    JsErrorCode errorCode = JsHasException(&hasException);
    if (errorCode != JsNoError) {
      // If script is in disabled state, no need to fail here.
      // We will fail subsequent calls to Jsrt anyway.
      assert(errorCode == JsErrorInDisabledState);
    } else if (hasException) {
      JsValueRef exceptionRef;
      errorCode = JsGetAndClearException(&exceptionRef);
      // We came here through JsHasException, so script shouldn't be in disabled
      // state.
      assert(errorCode != JsErrorInDisabledState);
      if (errorCode == JsNoError) {
        error = exceptionRef;
      }
    }
  }
  if(error != JS_INVALID_REFERENCE) {
    cbcatch(e, data);
    return true;
  }
  JsErrorCode errorCode = JsHasException(&hasException);
  if (errorCode != JsNoError) {
    if (errorCode == JsErrorInDisabledState) {
      cbcatch(e, data);
      return true;
    }
    // Should never get errorCode other than JsNoError/JsErrorInDisabledState
    assert(false);
    return false;
  }
  if (hasException) {
    cbcatch(e, data);
    return true;
  }
  return false;
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

napi_value napi_buffer_new(napi_env e, char* data, uint32_t size) {
  // TODO(tawoll): Replace v8impl with jsrt-based version.

  return v8impl::JsValueFromV8LocalValue(
    node::Buffer::New(
      v8impl::V8IsolateFromJsEnv(e), data, size).ToLocalChecked());
}

napi_value napi_buffer_copy(napi_env e, const char* data, uint32_t size) {
  // TODO(tawoll): Implement node::Buffer in terms of napi to avoid using chakra shim here.

  return v8impl::JsValueFromV8LocalValue(
    node::Buffer::Copy(
      v8impl::V8IsolateFromJsEnv(e), data, size).ToLocalChecked());
}

bool napi_buffer_has_instance(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsTypedArrayType arrayType;
  error = JsGetTypedArrayInfo(typedArray, &arrayType, nullptr, nullptr, nullptr);
  return (arrayType == JsArrayTypeUint8);
}

char* napi_buffer_data(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  JsValueRef arrayBuffer;
  unsigned int byteOffset;
  ChakraBytePtr buffer;
  unsigned int bufferLength;
  error = JsGetTypedArrayInfo(typedArray, nullptr, &arrayBuffer, &byteOffset, nullptr);
  error = JsGetArrayBufferStorage(arrayBuffer, &buffer, &bufferLength);
  return reinterpret_cast<char*>(buffer + byteOffset);
}

size_t napi_buffer_length(napi_env e, napi_value v) {
  JsErrorCode error = JsNoError;
  JsValueRef typedArray = reinterpret_cast<JsValueRef>(v);
  unsigned int len;
  error = JsGetTypedArrayInfo(typedArray, nullptr, nullptr, nullptr, &len);
  if (error != JsNoError)
    return 0;
  return len;
}
