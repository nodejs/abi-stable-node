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
  void* nm_link;
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
NODE_EXTERN napi_status napi_create_string_utf8(napi_env e, const char* s,
                                                int length, napi_value* result);
NODE_EXTERN napi_status napi_create_string_utf16(napi_env e, const char16_t* s,
                                                 int length, napi_value* result);
NODE_EXTERN napi_status napi_create_boolean(napi_env e, bool b, napi_value* result);
NODE_EXTERN napi_status napi_create_symbol(napi_env e, const char* s, napi_value* result);
NODE_EXTERN napi_status napi_create_function(napi_env e, napi_callback cb,
                                                 void* data, napi_value* result);
NODE_EXTERN napi_status napi_create_error(napi_env e, napi_value msg, napi_value* result);
NODE_EXTERN napi_status napi_create_type_error(napi_env e, napi_value msg, napi_value* result);
NODE_EXTERN napi_status napi_create_range_error(napi_env e, napi_value msg, napi_value* result);

// Methods to get the the native napi_value from Primitive type
NODE_EXTERN napi_status napi_get_type_of_value(napi_env e, napi_value vv, napi_valuetype* result);
NODE_EXTERN napi_status napi_get_value_double(napi_env e, napi_value v, double* result);
NODE_EXTERN napi_status napi_get_value_int32(napi_env e, napi_value v, int32_t* result);
NODE_EXTERN napi_status napi_get_value_uint32(napi_env e, napi_value v, uint32_t* result);
NODE_EXTERN napi_status napi_get_value_int64(napi_env e, napi_value v, int64_t* result);
NODE_EXTERN napi_status napi_get_value_bool(napi_env e, napi_value v, bool* result);

// Gets the number of CHARACTERS in the string.
NODE_EXTERN napi_status napi_get_value_string_length(napi_env e, napi_value v, int* result);

// Gets the number of BYTES in the UTF-8 encoded representation of the string.
NODE_EXTERN napi_status napi_get_value_string_utf8_length(napi_env e, napi_value v,
                                               int* result);

// Copies UTF-8 encoded bytes from a string into a buffer.
NODE_EXTERN napi_status napi_get_value_string_utf8(napi_env e, napi_value v,
                                               char* buf, int bufsize, int* result);

// Gets the number of 2-byte code units in the UTF-16 encoded representation of the string.
NODE_EXTERN napi_status napi_get_value_string_utf16_length(napi_env e, napi_value v,
                                               int* result);

// Copies UTF-16 encoded bytes from a string into a buffer.
NODE_EXTERN napi_status napi_get_value_string_utf16(napi_env e, napi_value v,
                                               char16_t* buf, int bufsize, int* result);

// Methods to coerce values
// These APIs may execute user script
NODE_EXTERN napi_status napi_coerce_to_bool(napi_env e, napi_value v, napi_value* result);
NODE_EXTERN napi_status napi_coerce_to_number(napi_env e, napi_value v, napi_value* result);
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
NODE_EXTERN napi_status napi_define_properties(napi_env e,
                                               napi_value object,
                                               int property_count,
                                               const napi_property_descriptor* properties);

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
                                  napi_value* buffer, int bufferlength);
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

NODE_EXTERN napi_status napi_define_class(napi_env e,
                                          const char* utf8name,
                                          napi_callback constructor,
                                          void* data,
                                          int property_count,
                                          const napi_property_descriptor* properties,
                                          napi_value* result);

// Methods to work with external data objects
NODE_EXTERN napi_status napi_wrap(napi_env e,
                                  napi_value jsObject,
                                  void* nativeObj,
                                  napi_finalize finalize_cb,
                                  napi_ref* result);
NODE_EXTERN napi_status napi_unwrap(napi_env e, napi_value jsObject, void** result);

NODE_EXTERN napi_status napi_create_external(napi_env e,
                                             void* data,
                                             napi_finalize finalize_cb,
                                             napi_value* result);
NODE_EXTERN napi_status napi_get_value_external(napi_env e, napi_value v, void** result);

// Methods to control object lifespan

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
NODE_EXTERN napi_status napi_create_reference(napi_env e,
                                              napi_value value,
                                              int initial_refcount,
                                              napi_ref* result);

// Deletes a reference. The referenced value is released, and may be GC'd unless there
// are other references to it.
NODE_EXTERN napi_status napi_delete_reference(napi_env e, napi_ref ref);

// Increments the reference count, optionally returning the resulting count. After this call the
// reference will be a strong reference because its refcount is >0, and the referenced object is
// effectively "pinned". Calling this when the refcount is 0 and the object is unavailable
// results in an error.
NODE_EXTERN napi_status napi_reference_addref(napi_env e, napi_ref ref, int* result);

// Decrements the reference count, optionally returning the resulting count. If the result is
// 0 the reference is now weak and the object may be GC'd at any time if there are no other
// references. Calling this when the refcount is already 0 results in an error.
NODE_EXTERN napi_status napi_reference_release(napi_env e, napi_ref ref, int* result);

// Attempts to get a referenced value. If the reference is weak, the value might no longer be
// available, in that case the call is still successful but the result is NULL.
NODE_EXTERN napi_status napi_get_reference_value(napi_env e, napi_ref ref, napi_value* result);

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
NODE_EXTERN napi_status napi_throw_range_error(napi_env e, const char* msg);

// Methods to support catching exceptions
NODE_EXTERN napi_status napi_is_exception_pending(napi_env e, bool* result);
NODE_EXTERN napi_status napi_get_and_clear_last_exception(napi_env e, napi_value* result);

// Methods to provide node::Buffer functionality with napi types
NODE_EXTERN napi_status napi_buffer_new(napi_env e,
                                        char* data,
                                        size_t size,
                                        napi_value* result);
NODE_EXTERN napi_status napi_buffer_copy(napi_env e,
                                         const char* data,
                                         size_t size,
                                         napi_value* result);
NODE_EXTERN napi_status napi_buffer_has_instance(napi_env e, napi_value v, bool* result);
NODE_EXTERN napi_status napi_buffer_data(napi_env e, napi_value v, char** result);
NODE_EXTERN napi_status napi_buffer_length(napi_env e, napi_value v, size_t* result);

// Methods to work with array buffers and typed arrays
NODE_EXTERN napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result);
NODE_EXTERN napi_status napi_create_arraybuffer(napi_env env, size_t byte_length, void** data,
                                                napi_value* result);
NODE_EXTERN napi_status napi_create_external_arraybuffer(napi_env env, void* external_data,
                                                         size_t byte_length, napi_value* result);
NODE_EXTERN napi_status napi_get_arraybuffer_info(napi_env env, napi_value arraybuffer,
                                                  void** data, size_t* byte_length);
NODE_EXTERN napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result);
NODE_EXTERN napi_status napi_create_typedarray(napi_env env, napi_typedarray_type type,
                                               size_t length, napi_value arraybuffer,
                                               size_t byte_offset, napi_value* result);
NODE_EXTERN napi_status napi_get_typedarray_info(napi_env env, napi_value typedarray,
                                                 napi_typedarray_type* type, size_t* length,
                                                 void** data, napi_value* arraybuffer,
                                                 size_t* byte_offset);
} // extern "C"

#endif  // SRC_NODE_JSVMAPI_H__
