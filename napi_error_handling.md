# Error handling

N-API uses both return values and exceptions for error handling.  The sections
which follow explain the approach for each case.

## return values

All of the N-API functions share the same error handling pattern.  The
return type of all API functions is ```napi_status```.

The return value will be ```napi_ok``` if the request was successful and
no uncaught exception was thrown. If an error occurred AND an exception
was thrown, the napi_status value for the error will be returned.  If an
exception was thrown, and no error occurred, ```napi_pending_exception```
will be returned.

In cases were a return value other than ```napi_ok``` or
```napi_pending_exception``` is returned, you must call
```napi_is_exception_pending()``` to check if an exception is pending.
See the section on exceptions for more details.

The full set of possible napi_status values are defined
in ```napi_api_types.h``.

The napi_status return value provides an vm independent representation of
the error which occured.  In some cases it is useful to be able to get
more detailed information, including a string representing the error as well as
vm(engine) specific information.  **NOTE:**  you should not rely on the
content or format of any of the extended information as it is
not subject to SemVer and may change at any time. It is intended
only for logging purposes.

In order to retrieve this information, the following method is provided:

```C
NODE_EXTERN const napi_extended_error_info* napi_get_last_error_info();
```
and the format of napi_extended_error_info structure is as follows:

```C
struct napi_extended_error_info {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
};
```

```napi_get_last_error_info()``` returns the information for the last
N-API call that was made and the  ```error_code``` will match the
```napi_status``` returned by that call.  The ```error_message``` will
be a textual representation of the error that occurred.

## Exceptions

Any N-API function may result in a pending JavaScript exception.  This is
obviotusly the case for any function that may cause the execution of JavaScript
but N-API specifies that an exception may be pending on return from any
of the API functions.

If the ```napi_status``` returned by a function is ```napi_ok``` then no
exception is pending and no additional action is required.  If the
```napi_status``` returned is anything other than ```napi_ok``` or
```napi_pending_exception```, then you must call the following function in
order to determine if an exception is pending or not:

```C
NODE_EXTERN napi_status napi_is_exception_pending(napi_env e, bool* result);
```

and then check the value of ```result```.  

When an exception is pending you will generally follow one of two approaches.

The first appoach is to do any appropriate cleanup and then return so that
execution will return to JavaScript.  As part of the transition back to
JavaScript the exception will be thrown at the point in the JavaScript
code were the native method was invoked.  The behviour of most N-API calls
is unspecified while an exception is pending, and many will simply return
```napi_pending_exception```, so it is important to do as little as possible
and then return to JavaScript where the exeption can be handled.

The second approach is to try to handle the exception. There will be cases
were the native code can 'catch' the exception, take the appropriate action,
and then continue.  It is recommended that you only do this in specific cases
where you know you can safely handle the exception.  In these cases you can
use the following function to get and clear the exception:

```C
NODE_EXTERN napi_status napi_get_and_clear_last_exception(napi_env e,
                                                          napi_value* result);
```

On success, result will contain the reference to the exception.  If you
determine after retrieving the exception that you cannot handle it after all,
you can re-throw it with the following function:

```C
NODE_EXTERN napi_status napi_throw(napi_env e, napi_value error);
```

were error, is the exception to be thrown.

The following utility functions are also available in case your native code
needs to throw an exception or determine in an napi_value represents an
exception.

```C
NODE_EXTERN napi_status napi_throw_error(napi_env e, const char* msg);
NODE_EXTERN napi_status napi_throw_type_error(napi_env e, const char* msg);
NODE_EXTERN napi_status napi_throw_range_error(napi_env e, const char* msg);
NODE_EXTERN napi_status napi_is_error(napi_env e,
                                      napi_value value,
                                      bool* result);
```
