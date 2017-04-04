# N-API - Basics

N-API is an API exposed by Node to developers that allow them to author Native 
add-ons in an ABI stable manner. This means that these add-ons can work 
against future versions of Node without recompilation, and developers get the 
additional benefit that their add-ons can work independent of the underlying 
JavaScript Virtual Machine being used by Node.

APIs exposed in the N-API surface are generally used to create and manipulate
JavaScript objects. Concepts and operations generally map to ideas specified 
in the ECMA262 Language Specification. The APIs generally have the following 
properties:
- All N-API APIs generally return a status code of type `napi_status`. This
  status can be used to determine whether the API call succeeded or failed.
- The actual value being returned by the API is returned in an out parameter.
- All JavaScript values are abstracted behind an opaque type named 
  `napi_value`.
- In case of an error status code, additional information can be obtained 
  using `napi_get_last_error_info`

## N-API types

N-API exposes the following fundamental datatypes as abstractions that are 
consumed by the various APIs. These APIs should be treated as opaque types 
that can be introspected only with other N-API APIs.

### *napi_env*

`napi_env` is used to represent a context that the underlying N-API 
implementation can use to persist VM specific state.
<< REVIEW: What else can we say here? >>

### *napi_value*
This is an opaque pointer that is used to represent a JavaScript value.

### *napi_ref*
This is the abstraction to use to reference a `napi_value`. This allows for 
users to manage the lifetimes of JavaScript values, including defining their 
minimum lifetimes explicitly, and also being notified when they are 
garbage-collected.

### *napi_handle_scope*
This is an abstraction used to provide hints to the garbage collector about 
the location of handles to GC pointers. napi_handle_scope allows the user to 
provide a variable stack separate from the thread stack, in a manner that is 
efficient for scan in case the underlying VM uses a garbage collector with 
precise-semantics.
N-API values are created within the context of a handle scope- if the user 
does not create a handle scope, they are created within the default handle
scope. In the case of a libuv callback, there is no default handle scope so 
the user is required to supply one.
Handle scopes are created using `napi_open_handle_scope` and are destroyed 
using `napi_close_handle_scope`. Calling close can indicate to the GC that all
napi_values created during the lifetime of the handle scope are no longer 
referenced from the current stack frame.

Consider creating a `napi_handle_scope` per function if you create any handle 
other than just the return value. Otherwise, the handle created will leak to 
the parent handle scope which can lead to higher than expected (and necessary)
memory usage.

### *napi_escapable_handle_scope*
Escapable Handle Scopes are a special type of handle scope that are used to 
return values created within a particular handle scope to a parent scope.

### *napi_callback_info*
Opaque datatype that is passed to a callback function. It can be used for two
purposes:
- Get additional information about the context in which the callback was 
  invoked
- Set the return value of the callback

### *napi_callback*
Function pointer type for N-API APIs that call back into user-provided native 
code. Your callback function should satisfy the following signature:
```
typedef void (*napi_callback)(napi_env, napi_callback_info);
```

### *napi_finalize*
Function pointer type for N-API APIs that allows the user to be notified when 
externally-owned data is ready to be cleaned up becauuse the object that it 
was associated with is garbage collected. The user must provide a function
satisfying the following signature which would get called upon the object's
collection.
```
typedef void (*napi_finalize)(void* finalize_data, void* finalize_hint);
```

### *napi_status*
Integral status code indicating the success or failure of a N-API API call.
Currently, the following status codes are supported.
```
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_function_expected,
  napi_number_expected,
  napi_boolean_expected,
  napi_generic_failure,
  napi_pending_exception,
  napi_status_last
```
If additional information is required upon an API returning a failed status, 
it can be obtained by calling `napi_get_last_error_info`.

### *napi_extended_error_info*
#### Definition
```
struct napi_extended_error_info {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
};
```

#### Members
- error_message: UTF8-encoded string containing a VM-neutral description of 
  the error
- engine_reserved: Reserved for VM-specific error details. This is currently
  not implemented for any VM.
- engine_error_code: VM-specific error code
- error_code: the N-API status code that originated with the current error









