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
 *    even for operations such as asking for the type of a napi_value or retrieving a
 *    function napi_callback's arguments.
 *  - The V8 implementation of the API is roughly hacked together with no
 *    attention paid to error handling or fault tolerance.
 *  - Error handling in general is not included in the API at this time.
 *
 ******************************************************************************/
#ifndef SRC_NODE_JAVASCRIPT_H_
#define SRC_NODE_JAVASCRIPT_H_

#include "node.h"


// JSVM API types are all opaque pointers for ABI stability
typedef void* napi_env;
typedef void* napi_value;
typedef void* napi_persistent;
typedef napi_value propertyname;
typedef const void* napi_func_cb_info;
typedef void (*napi_callback)(napi_env, napi_func_cb_info);
typedef void napi_destruct(void*);

enum class napi_valuetype {
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
NODE_EXTERN napi_value napi_get_undefined_(napi_env e);
NODE_EXTERN napi_value napi_get_null(napi_env e);
NODE_EXTERN napi_value napi_get_false(napi_env e);
NODE_EXTERN napi_value napi_get_true(napi_env e);
NODE_EXTERN napi_value napi_get_global_scope(napi_env e); 


// Methods to create Primitive types/Objects
NODE_EXTERN napi_value napi_create_object(napi_env e);
NODE_EXTERN napi_value napi_create_number(napi_env e, double val);
NODE_EXTERN napi_value napi_create_string(napi_env e, const char*);
NODE_EXTERN napi_value napi_create_function(napi_env e, napi_callback cbinfo);
NODE_EXTERN napi_value napi_create_type_error(napi_env e, napi_value msg);


// Methods to get the the native napi_value from Primitive type
NODE_EXTERN napi_valuetype napi_get_type_of_value(napi_env e, napi_value v);
NODE_EXTERN double napi_get_number_from_value(napi_env e, napi_value v);


// Methods to work with Objects
NODE_EXTERN napi_value napi_get_prototype(napi_env e, napi_value object);
NODE_EXTERN propertyname napi_proterty_name(napi_env e, const char* utf8name);
NODE_EXTERN void napi_set_property(napi_env e, napi_value object, propertyname name, napi_value v);
NODE_EXTERN napi_value napi_get_property(napi_env e, napi_value object, propertyname name);


// Methods to work with Functions
NODE_EXTERN void napi_set_function_name(napi_env e, napi_value func, propertyname napi_value);
NODE_EXTERN napi_value napi_call_function(napi_env e, napi_value scope, napi_value func, int argc, napi_value* argv);
NODE_EXTERN napi_value napi_new_instance(napi_env e, napi_value cons, int argc, napi_value *argv);


// Methods to work with napi_callbacks
NODE_EXTERN int napi_get_cb_args_length(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN void napi_get_cb_args(napi_env e, napi_func_cb_info cbinfo, napi_value* buffer, size_t bufferlength);
NODE_EXTERN napi_value napi_get_cb_object(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN bool napi_is_construct_call(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN void napi_set_return_value(napi_env e, napi_func_cb_info cbinfo, napi_value v);


// Methods to support ObjectWrap
NODE_EXTERN napi_value napi_create_constructor_for_wrap(napi_env e, napi_callback cb);
NODE_EXTERN void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj, napi_destruct* napi_destructor);
NODE_EXTERN void* napi_unwrap(napi_env e, napi_value jsObject);


// Methods to control object lifespan
NODE_EXTERN napi_persistent napi_create_persistent(napi_env e, napi_value v);
NODE_EXTERN napi_value napi_get_persistent_value(napi_env e, napi_persistent);


// Methods to support error handling
NODE_EXTERN void napi_throw_error(napi_env e, napi_value error);

///////////////////////////////////////////
// WILL GO AWAY
///////////////////////////////////////////
  typedef void(*workaround_init_napi_callback)(napi_env napi_env, napi_value exports, napi_value module);
  NODE_EXTERN void WorkaroundNewModuleInit(v8::Local<v8::Object> exports,
                                           v8::Local<v8::Object> module,
                                           workaround_init_napi_callback init);

  NODE_EXTERN v8::Local<v8::Value> V8LocalValue(napi_value v);
  NODE_EXTERN napi_value JsValue(v8::Local<v8::Value> v);

#endif  // SRC_NODE_JSVMAPI_H_
