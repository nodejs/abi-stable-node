# N-API - JavaScript Data Types and Values 

N-API exposes a set of APIs to interrogate and set properties on JavaScript objects.
Some of these types are documented under [Section 7](https://tc39.github.io/ecma262/#sec-operations-on-objects) 
of the [ECMAScript Language Specification](https://tc39.github.io/ecma262/).

Fundamentally, all properties in N-API can be represented in one of the following forms:
- Named: a simple utf8 encoded string
- Integer-Indexed: an index value represented by `uint32_t`
- JavaScript Object: a complex JavaScript Object represented by `napi_value`

N-API values are represented by the type `napi_value`.
Any N-API API that requires a JavaScript object takes in a `napi_value`
however, it's the caller's responsibility to make sure that the 
`napi_value` in question is of the JavaScript type expected by the API.


## Functions

### *napi_get_property_names*

#### Signature
```
napi_status napi_get_property_names(napi_env env,
                                    napi_value object,
                                    napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[out] result`: A `napi_value` representing an arrray of JavaScript objects that represent the property names of the object

#### Return value
- `napi_ok` if the API succeeded.

### *napi_set_property*

#### Signature
```
napi_status napi_set_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value value);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  key`: The name of the property to set
- `[in]  value`: The property value

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_property*

#### Signature
```
napi_status napi_get_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  key`: The name of the property to set
- `[out] result`: The value of the property

#### Return value
- `napi_ok` if the API succeeded.

### *napi_has_property*

#### Signature
```
napi_status napi_has_property(napi_env env,
                              napi_value object,
                              napi_value key,
                              bool* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  key`: The name of the property to set
- `[out] result`: Whether the property exists on the object or not

#### Return value
- `napi_ok` if the API succeeded.

### *napi_set_named_property*

#### Signature
```
napi_status napi_set_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    napi_value value);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  utf8Name`: The name of the property to set
- `[in]  value`: The property value

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_named_property*

#### Signature
```
napi_status napi_get_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  utf8Name`: The name of the property to set
- `[out] result`: The value of the property

#### Return value
- `napi_ok` if the API succeeded.

### *napi_has_named_property*

#### Signature
```
napi_status napi_has_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    bool* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  utf8Name`: The name of the property to set
- `[out] result`: Whether the property exists on the object or not

#### Return value
- `napi_ok` if the API succeeded.

### *napi_set_element*

#### Signature
```
napi_status napi_set_element(napi_env env,
                              napi_value object,
                              uint32_t index,
                              napi_value value);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  index`: The index of the property to set
- `[in]  value`: The property value

#### Return value
- `napi_ok` if the API succeeded.

### *napi_get_element*

#### Signature
```
napi_status napi_get_element(napi_env env,
                              napi_value object,
                              uint32_t index,
                              napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  index`: The index of the property to set
- `[out] result`: The value of the property

#### Return value
- `napi_ok` if the API succeeded.

### *napi_has_element*

#### Signature
```
napi_status napi_has_element(napi_env env,
                              napi_value object,
                              uint32_t index,
                              bool* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  index`: The index of the property to set
- `[out] result`: Whether the property exists on the object or not

#### Return value
- `napi_ok` if the API succeeded.

#### Signature
```
napi_status napi_define_properties(napi_env env,
                                   napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor* properties);
```

#### Parameters
- `[in]  env`: The environment that the N-API API is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  property_count`: The number of elements in the `properties` array
- `[in]  properties`: Array of property descriptors

#### Description
This method allows you to efficiently set multiple properties on a given object.
The properties are defined using property descriptors, which are defined in [Section 6.2.5](https://tc39.github.io/ecma262/#sec-property-descriptor-specification-type) of the ECMA262 specification.
Given an array of such property descriptors, this API will set the properties on the object one at a time, as defined by DefineOwnProperty (described in [Section 9.1.6](https://tc39.github.io/ecma262/#sec-ordinary-object-internal-methods-and-internal-slots-defineownproperty-p-desc) of the ECMA262 specification)