/*******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * This is a rough proof of concept not intended for real world usage.
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
#ifndef SRC_NODE_JAVASCRIPT_H_
#define SRC_NODE_JAVASCRIPT_H_

#include "node.h"


namespace node {
namespace js {

// JSVM API types are all opaque pointers for ABI stability
typedef void* env;
typedef void* value;
typedef void* persistent;
typedef value propertyname;
typedef const void* FunctionCallbackInfo;
typedef void (*callback)(env, FunctionCallbackInfo);
typedef void destruct(void*);

enum class valuetype {
    // ES6 types (corresponds to typeof)
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Symbol,
    Object,
    Function,
};


// Getters for defined singletons
NODE_EXTERN value GetUndefined(env e);
NODE_EXTERN value GetNull(env e);
NODE_EXTERN value GetFalse(env e);
NODE_EXTERN value GetTrue(env e);
NODE_EXTERN value GetGlobalScope(env e); 


// Methods to create Primitive types/Objects
NODE_EXTERN value CreateObject(env e);
NODE_EXTERN value CreateNumber(env e, double val);
NODE_EXTERN value CreateString(env e, const char*);
NODE_EXTERN value CreateFunction(env e, callback cbinfo);
NODE_EXTERN value CreateTypeError(env e, value msg);


// Methods to get the the native value from Primitive type
NODE_EXTERN valuetype GetTypeOfValue(env e, value v);
NODE_EXTERN double GetNumberFromValue(env e, value v);


// Methods to work with Objects
NODE_EXTERN value GetPrototype(env e, value object);
NODE_EXTERN propertyname PropertyName(env e, const char* utf8name);
NODE_EXTERN void SetProperty(env e, value object, propertyname name, value v);
NODE_EXTERN value GetProperty(env e, value object, propertyname name);


// Methods to work with Functions
NODE_EXTERN void  SetFunctionName(env e, value func, propertyname value);
NODE_EXTERN value Call(env e, value scope, value func, int argc, value* argv);
NODE_EXTERN value NewInstance(env e, value cons, int argc, value *argv);


// Methods to work with callbacks
NODE_EXTERN int GetCallbackArgsLength(env e, FunctionCallbackInfo cbinfo);
NODE_EXTERN void GetCallbackArgs(env e, FunctionCallbackInfo cbinfo, value* buffer, size_t bufferlength);
NODE_EXTERN value GetCallbackObject(env e, FunctionCallbackInfo cbinfo);
NODE_EXTERN bool IsContructCall(env e, FunctionCallbackInfo cbinfo);
NODE_EXTERN void SetReturnValue(env e, FunctionCallbackInfo cbinfo, value v);


// Methods to support ObjectWrap
NODE_EXTERN value CreateConstructorForWrap(env e, callback cb);
NODE_EXTERN void Wrap(env e, value jsObject, void* nativeObj, destruct* destructor);
NODE_EXTERN void* Unwrap(env e, value jsObject);


// Methods to control object lifespan
NODE_EXTERN persistent CreatePersistent(env e, value v);
NODE_EXTERN value GetPersistentValue(env e, persistent);


// Methods to support error handling
NODE_EXTERN void ThrowError(env e, value error);

///////////////////////////////////////////
// WILL GO AWAY
///////////////////////////////////////////
namespace legacy {
  typedef void(*workaround_init_callback)(env env, value exports, value module);
  NODE_EXTERN void WorkaroundNewModuleInit(v8::Local<v8::Object> exports,
                                           v8::Local<v8::Object> module,
                                           workaround_init_callback init);

  NODE_EXTERN v8::Local<v8::Value> V8LocalValue(value v);
  NODE_EXTERN value JsValue(v8::Local<v8::Value> v);
}  // namespace legacy
}  // namespace js
}  // namespace node

#endif  // SRC_NODE_JSVMAPI_H_
