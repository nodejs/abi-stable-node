# N-API - Abstract Operations 

N-API exposes a set of APIs to perform some abstract operations on JavaScript 
values. Some of these operations are documented under 
[Section 7](https://tc39.github.io/ecma262/#sec-abstract-operations) 
of the [ECMAScript Language Specification](https://tc39.github.io/ecma262/).

These APIs allow you to do one of the following:
1. Coerce JavaScript values to specific JavaScript types (such as Number or
   String)
2. Check the type of a JavaScript value
3. Check for equality between two JavaScript values

## Functions

### *napi_coerce_to_bool*

#### Signature
```C
napi_status napi_coerce_to_bool(napi_env env, 
                                napi_value value, 
                                napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to coerce
- `[out] result`: `napi_value` representing the coerced JavaScript Boolean

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API implements the abstract operation ToBoolean as defined in 
[Section 7.1.2](https://tc39.github.io/ecma262/#sec-toboolean)
of the ECMAScript Language Specification. 
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_number*

#### Signature
```C
napi_status napi_coerce_to_number(napi_env env, 
                                  napi_value value, 
                                  napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to coerce
- `[out] result`: `napi_value` representing the coerced JavaScript Number

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API implements the abstract operation ToNumber as defined in 
[Section 7.1.3](https://tc39.github.io/ecma262/#sec-tonumber)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_object*

#### Signature
```C
napi_status napi_coerce_to_object(napi_env env, 
                                  napi_value value,
                                  napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to coerce
- `[out] result`: `napi_value` representing the coerced JavaScript Object

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API implements the abstract operation ToObject as defined in 
[Section 7.1.13](https://tc39.github.io/ecma262/#sec-toobject)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_coerce_to_string*

#### Signature
```C
napi_status napi_coerce_to_string(napi_env env, 
                                  napi_value value, 
                                  napi_value* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to coerce
- `[out] result`: `napi_value` representing the coerced JavaScript String

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API implements the abstract operation ToString as defined in 
[Section 7.1.13](https://tc39.github.io/ecma262/#sec-tostring)
of the ECMAScript Language Specification.
This API can be re-entrant if getters are defined on the passed-in Object.

### *napi_typeof*

#### Signature
```C
napi_status napi_typeof(napi_env env, napi_value value, napi_valuetype* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value whose type to query
- `[out] result`: The type of the JavaScript value

#### Return value
- `napi_ok` if the API succeeded.
- `napi_invalid_arg` if the type of `value` is not a known ECMAScript type and
 `value` is not an External value.

#### Description
This API represents behavior similar to invoking the `typeof` Operator on 
the object as defined in 
[Section 12.5.5](https://tc39.github.io/ecma262/#sec-typeof-operator)
of the ECMAScript Language Specification. However, it has support for 
detecting an External value. If `value` has a type that is invalid, an error is
returned.

### *napi_instanceof*

#### Signature
```C
napi_status napi_instanceof(napi_env env, 
                            napi_value object, 
                            napi_value constructor, 
                            bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  object`: The JavaScript value to check
- `[in]  constructor`: The JavaScript function object of the constructor 
function to check against
- `[out] result`: Whether `object instanceof constructor` is true.

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API represents invoking the `instanceof` Operator on the object as 
defined in 
[Section 12.10.4](https://tc39.github.io/ecma262/#sec-instanceofoperator)
of the ECMAScript Language Specification.

### *napi_is_array*

#### Signature
```C
napi_status napi_is_array(napi_env env, napi_value value, bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to check
- `[out] result`: Whether the given object is an array

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API represents invoking the `IsArray` operation on the object 
as defined in [Section 7.2.2](https://tc39.github.io/ecma262/#sec-isarray)
of the ECMAScript Language Specification.

### *napi_is_arraybuffer*

#### Signature
```C
napi_status napi_is_arraybuffer(napi_env env, napi_value value, bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to check
- `[out] result`: Whether the given object is an ArrayBuffer

#### Return value
- `napi_ok` if the API succeeded.

### *napi_is_buffer*

#### Signature
```C
napi_status napi_is_buffer(napi_env env, napi_value value, bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to check
- `[out] result`: Whether the given `napi_value` represents a `node::Buffer`
object

#### Return value
- `napi_ok` if the API succeeded.

### *napi_is_error*

#### Signature
```C
napi_status napi_is_error(napi_env env, napi_value value, bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to check
- `[out] result`: Whether the given `napi_value` represents an Error object

#### Return value
- `napi_ok` if the API succeeded.

### *napi_is_typedarray*

#### Signature
```C
napi_status napi_is_typedarray(napi_env env, napi_value value, bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  value`: The JavaScript value to check
- `[out] result`: Whether the given `napi_value` represents a TypedArray

#### Return value
- `napi_ok` if the API succeeded.

### *napi_strict_equals*

#### Signature
```C
napi_status napi_strict_equals(napi_env env,
                               napi_value lhs,
                               napi_value rhs,
                               bool* result)
```

#### Parameters
- `[in]  env`: The environment that the API is invoked under
- `[in]  lhs`: The JavaScript value to check
- `[in]  rhs`: The JavaScript value to check against
- `[out] result`: Whether the two `napi_value` objects are equal

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This API represents the invocation of the Strict Equality algorithm as 
defined in 
[Section 7.2.14](https://tc39.github.io/ecma262/#sec-strict-equality-comparison) 
of the ECMAScript Language Specification.