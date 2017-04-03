# N-API Function APIs 

N-API provides a set of APIs that allow JavaScript code to 
call back into native code. N-API APIs that support calling back
into JavaScript take in callback function represented by 
the `napi_callback` type. When the JavaScript VM calls back to
native code, the `napi_callback` provided is invoked. The APIs 
documented in this section allow the callback function to do the 
following:
- Get information about the context in which the callback was invoked
- Get the arguments passed into the callback
- Return a `napi_value` back from the callback

Additionally, N-API provides a set of functions which allow calling 
JavaScript functions from native code. One can either call a function
like a regular JavaScript function call, or as a constructor 
function

## Functions

### *napi_call_function*

#### Signature
```
napi_status napi_call_function(napi_env env,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               const napi_value* argv,
                               napi_value* result)
```

#### Parameters
- `[in] e `: The environment that the API is invoked under
- `[in]  recv`: The this object passed to the called function
- `[in]  func`: `napi_value` representing the JavaScript function
to be invoked
- `[in]  argc`: The count of elements in the argv array
- `[in]  argv`: Array of JavaScript objects as `napi_value` 
representing the arguments to the function
- `[out] result`: The JavaScript object representing the return 
value from the function call

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_cb_args*

#### Signature
```
napi_status napi_get_cb_args(napi_env env,
                             napi_callback_info cbinfo,
                             napi_value* buffer,
                             size_t bufferlength)
```

#### Parameters
- `[in] e`: The environment that the API is invoked under
- `[in] cbinfo`: The callback info passed into the callback function
- `[in] buffer`: Array of `napi_value` into which the arguments to the 
callback function will be copied. This array must be user-allocated
and have space to store at least `bufferlength`-count elements
- `[in] bufferlength`: The size in elements in the buffer array

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_cb_data*

#### Signature
```
napi_status napi_get_cb_data(napi_env env,
                             napi_callback_info cbinfo,
                             void** result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[out] result`: The pointer to the `data` parameter that was passed
in when the callback was initially installed

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_cb_this*

#### Signature
```
napi_status napi_get_cb_this(napi_env env,
                             napi_callback_info cbinfo,
                             napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[out] result`: The `napi_value` representing the value of `this` 
provided for the call to the function.

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_cb_args_length*

#### Signature
```
napi_status napi_get_cb_args_length(napi_env env,
                                    napi_callback_info cbinfo,
                                    size_t* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[out] result`: The number of actual arguments passed into the function

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_cb_info*

#### Signature
```
napi_status napi_get_cb_info(napi_env env,
                             napi_callback_info cbinfo,
                             size_t* argc, 
                             napi_value* argv,
                             napi_value* thisArg, 
                             void** data)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[in-out] argc`: Specifies the size of the provided argv array
and receives the actual count of args.
- `[out] argv`: Buffer to which the `napi_value` representing the
arguments are copied
- `[out] this`: Receives the JS `this` arg for the call
- `[out] data`: Receives the data pointer for the callback.

#### Return value
- `napi_ok` if the API succeeded.

### *napi_is_construct_call*

#### Signature
```
napi_status napi_is_construct_call(napi_env env,
                                   napi_callback_info cbinfo,
                                   bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[out] result`: Whether the native function is being invoked as 
a constructor call

#### Return value
- `napi_ok` if the API succeeded.

### *napi_set_return_value*

#### Signature
```
napi_status napi_set_return_value(napi_env env,
                                  napi_callback_info cbinfo,
                                  napi_value value)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cbinfo`: The callback info passed into the callback function
- `[in]  value`: The value to return from the callback function

#### Return value
- `napi_ok` if the API succeeded.

### *napi_new_instance*

#### Signature
```
napi_status napi_new_instance(napi_env env,
                              napi_value cons,
                              size_t argc,
                              napi_value* argv,
                              napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  cons`: `napi_value` representing the JavaScript function
to be invoked as a constructor
- `[in]  argc`: The count of elements in the argv array
- `[in]  argv`: Array of JavaScript objects as `napi_value` 
representing the arguments to the function
- `[out] result`: The JavaScript object representing the return 
value from the function call, which in this case is the constructed
object

#### Return value
- `napi_ok` if the API succeeded.
