﻿/*******************************************************************************
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
 *  - The V8 implementation of the API is roughly hacked together with only basic
 *    error handling or fault tolerance.
 *
 ******************************************************************************/
#ifndef SRC_NODE_JSVMAPI_H_
#define SRC_NODE_JSVMAPI_H_

#include "node_jsvmapi_types.h"
#include <stdlib.h>
#include <stdint.h>

#ifndef NODE_EXTERN
# ifdef _WIN32
#   ifndef BUILDING_NODE_EXTENSION
#     define NODE_EXTERN __declspec(dllexport)
#   else
#     define NODE_EXTERN __declspec(dllimport)
#   endif
# else
#   define NODE_EXTERN /* nothing */
# endif
#endif

namespace node {
NODE_EXTERN typedef void (*addon_abi_register_func)(
  napi_env env,
  napi_value exports,
  napi_value module,
  void* priv);
}  // namespace node

struct napi_module_struct {
  int nm_version;
  unsigned int nm_flags;
  void* nm_dso_handle;
  const char* nm_filename;
  node::addon_abi_register_func nm_register_func;
  void* nm_context_register_func;
  const char* nm_modname;
  void* nm_priv;
  struct node_module* nm_link;
};

NODE_EXTERN void napi_module_register(void* mod);

#ifndef NODE_MODULE_EXPORT
# ifdef _WIN32
#   define NODE_MODULE_EXPORT __declspec(dllexport)
# else
#   define NODE_MODULE_EXPORT __attribute__((visibility("default")))
# endif
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define NODE_ABI_CTOR(fn)                                             \
  static void __cdecl fn(void);                                       \
  __declspec(dllexport, allocate(".CRT$XCU"))                         \
      void (__cdecl*fn ## _)(void) = fn;                              \
  static void __cdecl fn(void)
#else
#define NODE_ABI_CTOR(fn)                                             \
  static void fn(void) __attribute__((constructor));                  \
  static void fn(void)
#endif

#define NODE_MODULE_ABI_X(modname, regfunc, priv, flags)              \
  extern "C" {                                                        \
    static napi_module_struct _module =                               \
    {                                                                 \
      -1,                                                             \
      flags,                                                          \
      NULL,                                                           \
      __FILE__,                                                       \
      (node::addon_abi_register_func)regfunc,                         \
      NULL,                                                           \
      #modname,                                                       \
      priv,                                                           \
      NULL                                                            \
    };                                                                \
    NODE_ABI_CTOR(_register_ ## modname) {                            \
      napi_module_register(&_module);                                 \
    }                                                                 \
  }

#define NODE_MODULE_ABI(modname, regfunc)                             \
  NODE_MODULE_ABI_X(modname, regfunc, NULL, 0)


// TODO(ianhall): We're using C linkage for the API but we're also using the
// bool type in these exports.  Is that safe and stable?
extern "C" {
enum napi_valuetype {
  // ES6 types (corresponds to typeof)
  napi_undefined,
  napi_null,
  napi_boolean,
  napi_number,
  napi_string,
  napi_symbol,
  napi_object,
  napi_function,
};

enum napi_status {
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_function_expected,
  napi_generic_failure,
  napi_pending_exception,
  napi_status_last
};

struct napi_extended_error_info {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;

  napi_extended_error_info() :
    error_message(NULL),
    engine_reserved(NULL),
    engine_error_code(0),
    error_code(napi_ok)
  { }
};

NODE_EXTERN const napi_extended_error_info* napi_get_last_error_info();

// Environment
NODE_EXTERN napi_status napi_get_current_env(napi_env* e);

// Getters for defined singletons
NODE_EXTERN napi_status napi_get_undefined(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_get_null(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_get_false(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_get_true(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_get_global(napi_env e, napi_value* result);

// Methods to create Primitive types/Objects
NODE_EXTERN napi_status napi_create_object(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_create_array(napi_env e, napi_value* result);
NODE_EXTERN napi_status napi_create_array_with_length(napi_env e, int length,
                                                        napi_value* result);
NODE_EXTERN napi_status napi_create_number(napi_env e, double val, napi_value* result);
NODE_EXTERN napi_status napi_create_string(napi_env e, const char* s, napi_value* result);
NODE_EXTERN napi_status napi_create_string_with_length(napi_env e, const char* s,
                                                    size_t length, napi_value* result);
NODE_EXTERN napi_status napi_create_boolean(napi_env e, bool b, napi_value* result);
NODE_EXTERN napi_status napi_create_symbol(napi_env e, const char* s, napi_value* result);
NODE_EXTERN napi_status napi_create_function(napi_env e, napi_callback cb,
                                                 void* data, napi_value* result);
NODE_EXTERN napi_status napi_create_error(napi_env e, napi_value msg, napi_value* result);
NODE_EXTERN napi_status napi_create_type_error(napi_env e, napi_value msg, napi_value* result);

// Methods to get the the native napi_value from Primitive type
NODE_EXTERN napi_status napi_get_type_of_value(napi_env e, napi_value vv, napi_valuetype* result);
NODE_EXTERN napi_status napi_get_number_from_value(napi_env e, napi_value v, double* result);
NODE_EXTERN napi_status napi_get_string_from_value(napi_env e, napi_value v,
                                           char* buf, const int buf_size, int* result);
NODE_EXTERN napi_status napi_get_value_int32(napi_env e, napi_value v, int32_t* result);
NODE_EXTERN napi_status napi_get_value_uint32(napi_env e, napi_value v, uint32_t* result);
NODE_EXTERN napi_status napi_get_value_int64(napi_env e, napi_value v, int64_t* result);
NODE_EXTERN napi_status napi_get_value_bool(napi_env e, napi_value v, bool* result);

NODE_EXTERN napi_status napi_get_string_length(napi_env e, napi_value v, int* result);

// Do we need utf16 as well?
NODE_EXTERN napi_status napi_get_string_utf8_length(napi_env e, napi_value v, int* result);
NODE_EXTERN napi_status napi_get_string_utf8(napi_env e, napi_value v,
                                     char* buf, int bufsize, int* result);

// Methods to coerce values
// These APIs may execute user script
NODE_EXTERN napi_status napi_coerce_to_object(napi_env e, napi_value v, napi_value* result);
NODE_EXTERN napi_status napi_coerce_to_string(napi_env e, napi_value v, napi_value* result);


// Methods to work with Objects
NODE_EXTERN napi_status napi_get_prototype(napi_env e, napi_value object, napi_value* result);
NODE_EXTERN napi_status napi_property_name(napi_env e, const char* utf8name,
                                               napi_propertyname* result);
NODE_EXTERN napi_status napi_get_propertynames(napi_env e, napi_value object,
                                                   napi_value* result);
NODE_EXTERN napi_status napi_set_property(napi_env e, napi_value object,
                                   napi_propertyname name, napi_value v);
NODE_EXTERN napi_status napi_has_property(napi_env e, napi_value object,
                                   napi_propertyname name, bool* result);
NODE_EXTERN napi_status napi_get_property(napi_env e, napi_value object,
                                   napi_propertyname name, napi_value* result);
NODE_EXTERN napi_status napi_set_element(napi_env e, napi_value object,
                                  uint32_t i, napi_value v);
NODE_EXTERN napi_status napi_has_element(napi_env e, napi_value object,
                                  uint32_t i, bool* result);
NODE_EXTERN napi_status napi_get_element(napi_env e, napi_value object,
                                  uint32_t i, napi_value* result);
NODE_EXTERN napi_status napi_define_property(napi_env e, napi_value object,
                                  napi_property_descriptor* property);

// Methods to work with Arrays
NODE_EXTERN napi_status napi_is_array(napi_env e, napi_value v, bool* result);
NODE_EXTERN napi_status napi_get_array_length(napi_env e,
                                   napi_value v, uint32_t* result);

// Methods to compare values
NODE_EXTERN napi_status napi_strict_equals(napi_env e, napi_value lhs,
                                   napi_value rhs, bool* result);

// Methods to work with Functions
NODE_EXTERN napi_status napi_set_function_name(napi_env e, napi_value func,
                                        napi_propertyname napi_value);
NODE_EXTERN napi_status napi_call_function(napi_env e,
                                           napi_value recv,
                                           napi_value func,
                                           int argc,
                                           napi_value* argv,
                                           napi_value* result);

NODE_EXTERN napi_status napi_new_instance(napi_env e,
                                          napi_value cons,
                                          int argc,
                                          napi_value* argv,
                                          napi_value* result);

NODE_EXTERN napi_status napi_instanceof(napi_env e, napi_value obj,
                                        napi_value cons, bool* result);

// Temporary method needed to support wrapping JavascriptObject in an external
// object wrapper capable of storing external data. This workaround is only
// required by ChakraCore and should be removed when we have a method of
// constructing external objects from the constructor function itself.
NODE_EXTERN napi_status napi_make_external(napi_env e, napi_value v, napi_value* result);

// Napi version of node::MakeCallback(...)
NODE_EXTERN napi_status napi_make_callback(napi_env e,
                                           napi_value recv,
                                           napi_value func,
                                           int argc,
                                           napi_value* argv,
                                           napi_value* result);

// Methods to work with napi_callbacks
NODE_EXTERN napi_status napi_get_cb_args_length(napi_env e,
                                  napi_callback_info cbinfo, int* result);
NODE_EXTERN napi_status napi_get_cb_args(napi_env e, napi_callback_info cbinfo,
                                  napi_value* buffer, size_t bufferlength);
NODE_EXTERN napi_status napi_get_cb_this(napi_env e,
                                  napi_callback_info cbinfo, napi_value* result);
// V8 concept; see note in .cc file
NODE_EXTERN napi_status napi_get_cb_holder(napi_env e,
                                  napi_callback_info cbinfo, napi_value* result);
NODE_EXTERN napi_status napi_get_cb_data(napi_env e,
                                  napi_callback_info cbinfo, void** result);
NODE_EXTERN napi_status napi_is_construct_call(napi_env e,
                                  napi_callback_info cbinfo, bool* result);
NODE_EXTERN napi_status napi_set_return_value(napi_env e,
                                  napi_callback_info cbinfo, napi_value v);

// Methods to support ObjectWrap
// Consider: current implementation for supporting ObjectWrap pattern is
// difficult to implement for other VMs because of the dependence on node
// core's node::ObjectWrap type which depends on v8 types and specifically
// requires the given v8 object to have an internal field count of >= 1.
// It is proving difficult in the chakracore version of these APIs to
// implement this natively in JSRT which means that maybe this isn't the
// best way to attach external data to a javascript object.  Perhaps
// instead NAPI should do an external data concept like JsSetExternalData
// and use that for "wrapping a native object".
NODE_EXTERN napi_status napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
                                  napi_destruct napi_destructor,
                                  napi_weakref* handle);
NODE_EXTERN napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result);

NODE_EXTERN napi_status napi_create_constructor(napi_env e,
                                                const char* utf8name,
                                                napi_callback cb,
                                                void* data,
                                                int property_count,
                                                napi_property_descriptor* properties,
                                                napi_value* result);

// Methods to control object lifespan
NODE_EXTERN napi_status napi_create_persistent(napi_env e, napi_value v,
                                               napi_persistent* result);
NODE_EXTERN napi_status napi_release_persistent(napi_env e, napi_persistent p);
NODE_EXTERN napi_status napi_get_persistent_value(napi_env e, napi_persistent p,
                                                  napi_value* result);

NODE_EXTERN napi_status napi_create_weakref(napi_env e, napi_value v,
                                            napi_weakref* result);
NODE_EXTERN napi_status napi_get_weakref_value(napi_env e, napi_weakref w,
                                               napi_value* result);
NODE_EXTERN napi_status napi_release_weakref(napi_env e, napi_weakref w);

NODE_EXTERN napi_status napi_open_handle_scope(napi_env e, napi_handle_scope* result);
NODE_EXTERN napi_status napi_close_handle_scope(napi_env e, napi_handle_scope s);
NODE_EXTERN napi_status napi_open_escapable_handle_scope(napi_env e,
                                                napi_escapable_handle_scope* result);
NODE_EXTERN napi_status napi_close_escapable_handle_scope(napi_env e,
                                                          napi_escapable_handle_scope s);
NODE_EXTERN napi_status napi_escape_handle(napi_env e,
                                           napi_escapable_handle_scope s,
                                           napi_value v,
                                           napi_value* result);

// Methods to support error handling
NODE_EXTERN napi_status napi_throw(napi_env e, napi_value error);
NODE_EXTERN napi_status napi_throw_error(napi_env e, const char* msg);
NODE_EXTERN napi_status napi_throw_type_error(napi_env e, const char* msg);

// Methods to support catching exceptions
NODE_EXTERN napi_status napi_is_exception_pending(napi_env e, bool* result);
NODE_EXTERN napi_status napi_get_and_clear_last_exception(napi_env e, napi_value* result);

// Methods to provide node::Buffer functionality with napi types
NODE_EXTERN napi_status napi_buffer_new(napi_env e,
                                        char* data,
                                        uint32_t size,
                                        napi_value* result);
NODE_EXTERN napi_status napi_buffer_copy(napi_env e,
                                         const char* data,
                                         uint32_t size,
                                         napi_value* result);
NODE_EXTERN napi_status napi_buffer_has_instance(napi_env e, napi_value v, bool* result);
NODE_EXTERN napi_status napi_buffer_data(napi_env e, napi_value v, char** result);
NODE_EXTERN napi_status napi_buffer_length(napi_env e, napi_value v, size_t* result);
} // extern "C"

#endif  // SRC_NODE_JSVMAPI_H__
