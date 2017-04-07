# N-API Object Wrap APIs

N-API offers a way to "wrap" C++ classes and instances so that the class
constructor and methods can be called from JavaScript.

 1. The `napi_define_class()` API defines a JavaScript class with constructor,
    static properties and methods, and instance properties and methods that
    correspond to the The C++ class.
 2. When JavaScript code invokes the constructor, the constructor callback
    uses `napi_wrap()` to wrap a new C++ instance in a JavaScript object,
    then returns the wrapper object.
 3. When JavaScript code invokes a method or property accessor on the class,
    the corresponding `napi_callback` C++ function is invoked. For an instance
    callback, `napi_unwrap()` obtains the C++ instance that is the target of
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
Defines a JavaScript class that corresponds to a C++ class, including:
 - A JavaScript constructor function that has the class name and invokes the
   provided C++ constructor callback
 - Properties on the constructor function corresponding to _static_ data
   properties, accessors, and methods of the C++ class (defined by
   property descriptors with the `napi_static` attribute)
 - Properties on the constructor function's `prototype` object corresponding to
   _non-static_ data properties, accessors, and methods of the C++ class
   (defined by property descriptors without the `napi_static` attribute)

The C++ constructor callback should be a static method on the class that calls
the actual class constructor, then wraps the new C++ instance in a JavaScript
object, and returns the wrapper object. See `napi_wrap()` for details.

The JavaScript constructor function returned from `napi_define_class()` is
often saved and used later, to construct new instances of the class from native
code, and/or check whether provided values are instances of the class. In that
case, to prevent the function value from being garbage-collected, create a
persistent reference to it using `napi_create_reference()` and ensure the
reference count is kept >= 1.

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
Wraps a native instance in JavaScript object of the corresponding type.

When JavaScript code invokes a constructor for a class that was defined using
`napi_define_class()`, the `napi_callback` for the constructor is invoked.
After constructing an instance of the native class, the callback must then call
`napi_wrap()` to wrap the newly constructed instance in the already-created
JavaScript object that is the `this` argument to the constructor callback.
(That `this` object was created from the constructor function's `prototype`,
so it already has definitions of all the instance properties and methods.)

Typically when wrapping a class instance, a finalize callback should be
provided that simply deletes the native instance that is received as the `data`
argument to the finalize callback.

The optional returned reference is initially a weak reference, meaning it
has a reference count of 0. Typically this reference count would be incremented
temporarily during async operations that require the instance to remain valid.

Caution: The optional returned reference (if obtained) should be deleted via
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
