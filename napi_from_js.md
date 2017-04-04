# N-API Function APIs 

N-API provides a set of APIs that allow JavaScript code to 
call back into native code. N-API APIs that support calling back
into native code take in a callback functions represented by 
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
function.

## Functions

### *napi_call_function*

#### Signature
```C
napi_status napi_call_function(napi_env env,
                               napi_value recv,
                               napi_value func,
                               int argc,
                               const napi_value* argv,
                               napi_value* result)
```

#### Parameters
- `[in]  env `: The environment that the API is invoked under
- `[in]  recv`: The `this` object passed to the called function
- `[in]  func`: `napi_value` representing the JavaScript function
to be invoked
- `[in]  argc`: The count of elements in the `argv` array
- `[in]  argv`: Array of JavaScript objects as `napi_value` 
representing the arguments to the function. If there are more arguments
than the provided argument count, then only the requested number of 
arguments are copied. If there are fewer arguments provided than 
claimed, the rest of `argv` is filled with `napi_value` values that 
represent `undefined`
- `[out] result`: The JavaScript object representing the return 
value from the function call

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This method allows you to call a JavaScript function object from your native 
add-on. This is the primary mechanism of calling back *from* the add-on's
native code *into* JavaScript.

A sample use case might look as follows. If you had the following JavaScript
consider the following snippet:
```js
function AddTwo(num)
{
    return num + 2;
}
```

Then, the above function can be invoked from your native add-on using the
following code:
```C
// Get the function named "AddTwo" on the global object
napi_value global, add_two, arg;
napi_status status = napi_get_global(env, &global);
if (status != napi_ok) return;

status = napi_get_named_property(env, global, "AddTwo", &add_two);
if (status != napi_ok) return;

// var arg = 1337
status = napi_create_number(env, 1337, &arg);
if (status != napi_ok) return;

napi_value* argv = &arg;
size_t argc = 1;

// AddTwo(arg);
napi_value return_val;
status = napi_call_function(env, global, add_two, argc, argv, &return_val);
if (status != napi_ok) return;

// Convert the result back to a native type
int32_t result;
status = napi_get_value_int32(env, return_val, &result);
if (status != napi_ok) return;
```

### *napi_create_function*

#### Signature
```C
napi_status napi_create_function(napi_env env,
                                 const char* utf8name,
                                 napi_callback cb,
                                 void* data,
                                 napi_value* result);
```

#### Parameters
- `[in]  env `: The environment that the API is invoked under
- `[in]  utf8Name`: The name of the function, encoded as UTF8. This is visible
within JavaScript as the new function object's `name` property.
- `[in]  cb`: The native function which should be called when this function 
object is invoked
- `[in]  data`: User-provided data context. This will be passed back into the 
function when invoked later.
- `[out] result`: `napi_value` representing the JavaScript function object for
the newly created function

#### Description
This API allows an add-on author to create a function object in native code. 
This is the primary mechanism to allow calling *into* the add-on's native code
*from* Javascript.

Note however, the newly created function is not automatically  visible from
script after this call. Instead, you must explicitly set a property on the
Global object (or any other object that is visible to JavaScript), in order for
the function to be accessible from script. For instance, if you wish to expose a
function as part of the add-on's module exports, you can set the newly created
function on the exports object. A sample module might look as follows:
```C
void SayHello(napi_env env, napi_callback_info info) {
  printf("Hello\n");
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value fn;
  status =  napi_create_function(env, NULL, SayHello, NULL, &fn);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "sayHello", fn);
  if (status != napi_ok) return;
}

NAPI_MODULE(add-on, Init)
```

Given the above code, the add-on can be used from JavaScript as follows:
```js
var myaddon = require('myadd-on');
myaddon.sayHello();
```

### *napi_get_cb_args*

#### Signature
```C
napi_status napi_get_cb_args(napi_env env,
                             napi_callback_info cbinfo,
                             napi_value* buffer,
                             size_t bufferlength)
```

#### Parameters
- `[in] env`: The environment that the API is invoked under
- `[in] cbinfo`: The callback info passed into the callback function
- `[in] buffer`: Array of `napi_value` into which the arguments to the 
callback function will be copied. This array must be user-allocated
and have space to store at least `bufferlength`-count elements. Any
additional space will be filled with `napi_value` values that represent 
`undefined`
- `[in] bufferlength`: The size in elements in the `buffer` array

#### Return value
- `napi_ok` if the API succeeded.

### Description
This method allows you to get the arguments provided to your native function
from JavaScript.

### *napi_get_cb_args_length*

#### Signature
```C
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

### *napi_get_cb_data*

#### Signature
```C
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

### Description
This method allows you to retrieve the data pointer that you had passed in to 
[`napi_create_function`](#napi_create_function) when you originally created
the function object.

### *napi_get_cb_this*

#### Signature
```C
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

#### Description
This method retrieves the `this` pointer that was passed in when the add-on's
native method was invoked from JavaScript.

### *napi_get_cb_info*

#### Signature
```C
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
- `[in-out] argc`: Specifies the size of the provided `argv` array
and receives the actual count of arguments.
- `[out] argv`: Buffer to which the `napi_value` representing the
arguments are copied
- `[out] this`: Receives the JS `this` argument for the call
- `[out] data`: Receives the data pointer for the callback.

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This is a convenience method that retrieves all relevant information from a 
`napi_callback_info` in a single call.

### *napi_is_construct_call*

#### Signature
```C
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
```C
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

#### Description
This API is used to set the value returned from an add-on-provided callback
function back to JavaScript.

### *napi_new_instance*

#### Signature
```C
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
- `[in]  argc`: The count of elements in the `argv` array
- `[in]  argv`: Array of JavaScript objects as `napi_value` 
representing the arguments to the function
- `[out] result`: The JavaScript object representing the return 
value from the function call, which in this case is the constructed
object

#### Description
This method is used to instantiate a new JavaScript Object using a given 
`napi_value` that represents the constructor for the object. For example, 
consider the following snippet:
```js
function MyObject(param)
{
    this.param = param;
}

var arg = "hello";
var value = new MyObject(arg);
```

The following can be approximated in N-API using the following snippet:
```C
// Get the constructor function MyObject
napi_value global, constructor, arg, value;
napi_status status = napi_get_global(env, &global);
if (status != napi_ok) return;

status = napi_get_named_property(env, global, "MyObject", &constructor);
if (status != napi_ok) return;

// var arg = "hello"
status = napi_create_string_utf8(env, "hello", -1, &arg);
if (status != napi_ok) return;

napi_value* argv = &arg;
size_t argc = 1;

// var value = new MyObject(arg)
status = napi_new_instance(env, constructor, argc, argv, &value);
```

#### Return value
- `napi_ok` if the API succeeded.
