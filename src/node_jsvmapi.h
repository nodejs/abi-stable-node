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

typedef void destruct(void*);

namespace node {
namespace js {

// JSVM API types are all opaque pointers for ABI stability
typedef void* env;

typedef void* value;
typedef void* persistent;
typedef value propertyname;

// V8 has the concept of a function's holder; should that be part of the
// VM agnostic API?
typedef const void* FunctionCallbackInfo;
typedef void (*callback)(env, FunctionCallbackInfo);

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

NODE_EXTERN valuetype GetTypeOfValue(env, value);

NODE_EXTERN value GetUndefined(env);
NODE_EXTERN value GetNull(env);
NODE_EXTERN value GetFalse(env);
NODE_EXTERN value GetTrue(env);

NODE_EXTERN value CreateObject(env);
NODE_EXTERN value CreateNumber(env, double);
NODE_EXTERN value CreateString(env, const char*);
NODE_EXTERN value CreateFunction(env, callback);
NODE_EXTERN value CreateConstructorForWrap(env, callback);
NODE_EXTERN void  SetFunctionName(env, value, value);

NODE_EXTERN value CreateTypeError(env, value msg);

NODE_EXTERN void ThrowError(env, value);

// These operations probably will hurt performance if they always have to
// across the DLL boundary.  FunctionCallbackInfo likely will need to be
// a defined type rather than an opaque pointer.
NODE_EXTERN int GetCallbackArgsLength(env, FunctionCallbackInfo);
// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
NODE_EXTERN void GetCallbackArgs(env, FunctionCallbackInfo, value* buffer, size_t bufferlength);
NODE_EXTERN value GetCallbackObject(env, FunctionCallbackInfo);
NODE_EXTERN bool IsContructCall(env, FunctionCallbackInfo);
NODE_EXTERN void SetReturnValue(env, FunctionCallbackInfo, value);

NODE_EXTERN propertyname PropertyName(env, const char*);
NODE_EXTERN void SetProperty(env, value object, propertyname, value);
NODE_EXTERN value GetProperty(env e, value o, propertyname k);

NODE_EXTERN value GetPrototype(env, value object);

// CONSIDER: SetProperties for bulk set operations

NODE_EXTERN double GetNumberFromValue(env, value);

NODE_EXTERN value GetGlobalScope(env); 
NODE_EXTERN value Call(env, value, value, int, value*);

NODE_EXTERN void Wrap(env e, value jsObject, void* nativeObj, destruct* destructor);
NODE_EXTERN void* Unwrap(env, value);

NODE_EXTERN persistent CreatePersistent(env, value);
NODE_EXTERN value GetPersistentValue(env, persistent);

NODE_EXTERN value NewInstance(env, value, int, value*);

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
