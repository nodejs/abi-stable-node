# N-API Object Wrap APIs

N-API offers a way to "wrap" C++ classes and instances so that they are
effectively projected into JavaScript.

 1. The C++ class constructor callback, static properties and methods, and
    instance properties methods are projected as a JavaScript class using
    `napi_define_class()`.
 2. When JavaScript code invokes the constructor, the constructor callback
    uses `napi_wrap()` to wrap a new C++ instance in a JavaScript wrapper,
    then returns the wrapper.
 3. When JavaScript code invokes a method or property accessor on the class,
    the corresponding `napi_callback` is invoked. For an instance callback,
    `napi_unwrap()` is used to obtain the C++ instance that is the target of
    the call.

### *napi_define_class*

#### Signature
```C
napi_status napi_define_class(napi_env env,
                              const char* utf8name,
                              napi_callback constructor,
                              void* data,
                              size_t property_count,
                              const napi_property_descriptor* properties,
                              napi_value* result);
```

#### Parameters
 - `[in]  env`: The environment that the API is invoked under
 - `[in]  utf8name`: Name of the JavaScript constructor function; this is
   not required to be the same as the C++ class name, though it is recommended
   for clarity
 - `[in]  constructor`: Callback function that handles constructing instances
   of the class. (This should be a static method on the class, not an actual
   C++ constructor function.)
 - `[in]  data`: Optional data to be passed to the constructor callback as
   the `data' property of the callback info
 - `[in]  property_count`: Number of items in the `properties` array argument
 - `[in]  properties`: Array of property descriptors describing static and
   instance data properties, accessors, and methods on the class
   See `napi_property_descriptor`
 - `[out] result`: A `napi_value` representing the constructor function for
   the class

#### Description
Projects a C++ class to JavaScript. Specifically, the C++ constructor callback
is projected as a JavaScript constructor function having the class name; static
data properties, accessors, and methods are projected as properties on the
JavaScript constructor function; and instance data properties, accessors, and
methods are projected as properties on the JavaScript constructor function's
`prototype`.

The constructor function returned from `napi_define_class()` is often saved and
used later, to construct new instances of the class from native code,and/or
check whether provided values are instances of the class. In that case, to
prevent the function value from being garbage-collected, create a persistent
reference to it using `napi_create_reference()` and ensure the reference count
is kept >= 1.

### *napi_wrap*

#### Signature
```C
napi_status napi_wrap(napi_env env,
                      napi_value js_object,
                      void* native_object,
                      napi_finalize finalize_cb,
                      void* finalize_hint,
                      napi_ref* result);
```

#### Parameters
 - `[in]  env`: The environment that the API is invoked under
 - `[in]  js_object`: The JavaScript object that will be the wrapper for the
   native object. This object _must_ have been created from the `prototype` of
   a constructor that was created using `napi_define_class()`.
 - `[in]  native_object`: The native instance that will be wrapped in the
   JavaScript object.
 - `[in]  finalize_cb`: Optional native callback that can be used to free the
   native instance when the JavaScript object is ready for garbage-collection
 - `[in]  finalize_hint`: Optional contextual hint that is passed to the
   finalize callback.
 - `[out] result`: Optional reference to the wrapped object

#### Description
When JavaScript code invokes a constructor for a class that was projected using `napi_define_class()`, the `napi_callback` for the constructor is invoked.
After constructing an instance of the native class, the callback must then call
`napi_wrap()` to wrap the newly constructed instance in the already-created
JavaScript object that is the `this` argument to the constructor callback.
(That `this` object was created from the constructor function's `prototype`,
so it already has projections of all the instance properties and methods.)

Typically when wrapping a class instance, a finalize callback should be
provided that simply calls `delete` on the native instance that is received as
the `data` argument to the finalize callback.

The optional returned reference is initially a weak reference, meaning it
has a reference count of 0. Typically this reference count would be incremented
temporarily during async operations that require the instance to remain valid.

Caution: The optional returned reference should be deleted via
`napi_delete_reference()` ONLY in response to the finalize callback invocation.
(If it is deleted before then, then the finalize callback may never be
invoked.) Therefore when obtaining a reference a finalize callback is also
required in order to enable correct proper of the reference.

### *napi_unwrap*

#### Signature
```C
napi_status napi_unwrap(napi_env env,
                        napi_value js_object,
                        void** result);
```

#### Parameters
 - `[in]  env`: The environment that the API is invoked under
 - `[in]  js_object`:
 - `[out] result`:

#### Description
When JavaScript code invokes a method or property accessor on the class, the
corresponding `napi_callback` is invoked. If the callback is for an instance
method or accessor, then the `this` argument to the callback is the wrapper
object; the wrapped C++ instance that is the target of the call can be obtained
then by calling `napi_unwrap()` on the wrapper object.
