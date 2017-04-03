# N-API - Basics

N-API is an API exposed by Node to developers that allow them to author native
add-ons in an ABI-stable manner. This means that these add-ons can work
against future versions of Node without recompilation, and developers get the
additional benefit that their add-ons can work independently of the underlying
JavaScript Virtual Machine being used by Node.

APIs exposed by the N-API surface are generally used to create and manipulate
JavaScript values. Concepts and operations generally map to ideas specified
in the ECMA262 Language Specification. The APIs generally have the following
properties:
- All N-API calls generally return a status code of type `napi_status`. This
  status can be used to determine whether the API call succeeded or failed.
- The actual value being returned by the API is returned in an out parameter.
- All JavaScript values are abstracted behind an opaque type named
  `napi_value`.
- In case of an error status code, additional information can be obtained
  using `napi_get_last_error_info`. More information can be found in the error
  handling section (<PLACEHOLDER: link to error handling doc>)

## N-API General types

N-API exposes the following fundamental datatypes as abstractions that are
consumed by the various APIs. These APIs should be treated as opaque,
introspectable only with other N-API calls.

### *napi_status*
Integral status code indicating the success or failure of a N-API call.
Currently, the following status codes are supported.
```C
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_function_expected,
  napi_number_expected,
  napi_boolean_expected,
  napi_generic_failure,
  napi_pending_exception
```
If additional information is required upon an API returning a failed status,
it can be obtained by calling `napi_get_last_error_info`.

### *napi_extended_error_info*
#### Definition
```C
typedef struct {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
} napi_extended_error_info;
```

#### Members
- error_message: UTF8-encoded string containing a VM-neutral description of
  the error
- engine_reserved: Reserved for VM-specific error details. This is currently
  not implemented for any VM.
- engine_error_code: VM-specific error code. This is currently
  not implemented for any VM.
- error_code: the N-API status code that originated with the last error

### Additional Information

See the Error Handling section (<PLACEHOLDER>)

### *napi_env*

`napi_env` is used to represent a context that the underlying N-API
implementation can use to persist VM-specific state. This structure is passed
to native functions when they're invoked, and it must be passed back when
making N-API calls. Specifically, the same `napi_env` that was passed in when
the initial native function was called must be passed to any subsequent
nested N-API calls. Caching the `napi_env` for the purpose of general reuse is
not allowed.

### *napi_value*
This is an opaque pointer that is used to represent a JavaScript value.

## N-API Memory Management types

### *napi_handle_scope*
This is an abstraction used to control and modify the lifetime of objects
created within a particular scope. In general, N-API values are created within
the context of a handle scope. When a native method is called from
JavaScript, a default handle scope will exist- if the user does not explicitly
create a new handle scope, N-API values will be created in the default handle
scope. For any invocations of code outside the execution of a native method
(for instance, during a libuv callback invocation), the module is required to
create a scope before invoking any functions that can result in the creation
of JavaScript values.

Handle scopes are created using `napi_open_handle_scope` and are destroyed
using `napi_close_handle_scope`. Closing the scope can indicate to the GC that
all `napi_value`s created during the lifetime of the handle scope are no longer
referenced from the current stack frame.

For more details, review the [Object Lifetime Management section](TODO://path).

### *napi_escapable_handle_scope*
Escapable handle scopes are a special type of handle scope to return values
created within a particular handle scope to a parent scope.

### *napi_ref*
This is the abstraction to use to reference a `napi_value`. This allows for
users to manage the lifetimes of JavaScript values, including defining their
minimum lifetimes explicitly.

For more details, review the [Object Lifetime Management section](TODO://path).

## N-API Callback types

### *napi_callback_info*
Opaque datatype that is passed to a callback function. It can be used for two
purposes:
- Get additional information about the context in which the callback was
  invoked
- Set the return value of the callback

### *napi_callback*
Function pointer type for user-provided native functions which are to be
exposed to JavaScript via N-API. Your callback function should satisfy the
following signature:
```C
typedef void (*napi_callback)(napi_env, napi_callback_info);
```

### *napi_finalize*
Function pointer type for add-on provided functions that allow the user to be
notified when externally-owned data is ready to be cleaned up because the
object with which it was associated with, has been garbage-collected. The user
must provide a function satisfying the following signature which would get
called upon the object's collection. Currently, `napi_finalize` can be used for
finding out when objects that have external data are collected.
```C
typedef void (*napi_finalize)(void* finalize_data, void* finalize_hint);
```
