# N-API - JavaScript Data Types and Values 

N-API exposes a set of APIs to get and set properties on JavaScript 
values. Some of these types are documented under 
[Section 7](https://tc39.github.io/ecma262/#sec-operations-on-objects) of the 
[ECMAScript Language Specification](https://tc39.github.io/ecma262/).

Properties in JavaScript are represented as a tuple of a key and a value. 
Fundamentally, all property keys in N-API can be represented in one of the
following forms:
- Named: a simple utf8 encoded string
- Integer-Indexed: an index value represented by `uint32_t`
- JavaScript value: these are represented in N-API by `napi_value`. This can
be a `napi_value` representing a String, Number or Symbol.

N-API values are represented by the type `napi_value`.
Any N-API call that requires a JavaScript value takes in a `napi_value`.
However, it's the caller's responsibility to make sure that the 
`napi_value` in question is of the JavaScript type expected by the API.

The APIs documented in this section provide a simple interface to allow you to 
get and set properties on arbitrary JavaScript objects represented by 
`napi_value`.

For instance, consider the following JavaScript code snippet:
```
var obj = {};
obj.myProp = 123;
```
You can do the equivalent using N-API values with the following snippet:
```
napi_status status = napi_status_generic_failure;

// var obj = {}
napi_value obj, value;
status = napi_create_object(env, &obj);
if (status != napi_ok) return status;

// Create a napi_value for 123
status = napi_create_number(env, 123, &value);
if (status != napi_ok) return status;

// obj.myProp = 123
status = napi_set_named_property(env, obj, "myProp", value);
if (status != napi_ok) return status;
```

You can also set index-properties in a similar manner. Consider the following
JavaScript snippet:
```
var arr = [];
arr[123] = 'hello';
```
You can do the equivalent using N-API values with the following snippet:
```
napi_status status = napi_status_generic_failure;

// var arr = [];
napi_value arr, value;
status = napi_create_array(env, &arr);
if (status != napi_ok) return status;

// Create a napi_value for 'hello'
status = napi_create_string_utf8(env, "hello", -1, &value);
if (status != napi_ok) return status;

// arr[123] = 'hello';
status = napi_set_element(env, arr, 123, value);
if (status != napi_ok) return status;
```

You can also get properties using the APIs described in this section.
Consider the following JavaScript snippet:
```
var arr = [];
return arr[123];
```

The following is the approximate equivalent of the N-API counterpart:
```
napi_status status = napi_status_generic_failure;

// var arr = []
napi_value arr, value;
status = napi_create_array(env, &arr);
if (status != napi_ok) return status;

// get arr[123]
status = napi_get_element(env, arr, 123, &value);
if (status != napi_ok) return status;
```

Finally, you can also define multiple properties on an object for performance
reasons. Consider the following JavaScript:
```
var obj = {};
Object.defineProperties(obj, {
  'foo': { value: 123, writable: true, configurable: true, enumerable: true },
  'bar': { value: 456, writable: true, configurable: true, enumerable: true }
});
```

The following is the approximate equivalent of the N-API counterpart:
```
napi_status status = napi_status_generic_failure;

// var obj = {};
napi_value obj;
status = napi_create_obj(env, &obj);
if (status != napi_ok) return status;

// Create napi_values for 123 and 456
napi_value fooValue, barValue;
status = napi_create_number(env, 123, &fooValue);
if (status != napi_ok) return status;
status = napi_create_number(env, 456, &barValue);
if (status != napi_ok) return status;

// Set the properties
napi_property_descriptors descriptors[] = {
  { "foo", fooValue, 0, 0, 0, napi_default, 0 },
  { "bar", barValue, 0, 0, 0, napi_default, 0 }
}
status = napi_define_properties(env,
                                obj,
                                sizeof(descriptors) / sizeof(descriptors[0]),
                                descriptors);
if (status != napi_ok) return status;
```

## Structures

### *napi_property_attributes*

#### Definition
```
typedef enum {
  napi_default = 0,
  napi_read_only = 1 << 0,
  napi_dont_enum = 1 << 1,
  napi_dont_delete = 1 << 2,
  napi_static_property = 1 << 10,
} napi_property_attributes;
```

### Description
`napi_property_attributes` are flags used to control the behavior of properties 
set on a JavaScript value. They roughly correspond to the attributes listed in
[Section 6.1.7.1](https://tc39.github.io/ecma262/#table-2) of the
[ECMAScript Language Specification](https://tc39.github.io/ecma262/).

`napi_default` - Used to indicate that no explicit attributes are set on the 
given property. By default, a property is Writable, Enumerable, and
 Configurable. Note that this is a deviation from the ECMAScript specification,
 where generally the values for a property descriptor attribute default to 
 false if they're not provided.
`napi_read_only` - Used to indicate that a given property is not Writable
`napi_dont_enum` - Used to indicate that a given property is not Enumerable
`napi_dont_delete` - Used to indicate that a given property is not 
Configurable, as defined in 
[Section 6.1.7.1](https://tc39.github.io/ecma262/#table-2) of the 
[ECMAScript Language Specification](https://tc39.github.io/ecma262/).
`napi_static_property` - Used to indicate that the property will be defined as
a static property on a class as opposed to an instance property, which is the
default. This is used only by `napi_define_class`. It is ignored by 
`napi_define_properties`

### *napi_property_descriptor*

#### Definition
```
typedef struct {
  const char* utf8name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
} napi_property_descriptor;
```

#### Members
- `utf8name`: String describing the key for the property, encoded as UTF8
- `value`: The value that's retrieved by a get access of the property if the
 property is a data. If this is passed in, set `getter`, `setter`, `method` 
 and `data` to `NULL` (since these members won't be used)
- `getter`: A function to call when a get access of the property is performed.
If this is passed in, set `value` and `method` to `NULL` (since these members
won't be used). The given function is called implicitly by the runtime when the
property is accessed from JavaScript code (or if a get on the property is 
performed using a N-API call)
- `setter`: A function to call when a set access of the property is performed.
If this is passed in, set `value` and `method` to `NULL` (since these members
won't be used). The given function is called implicitly by the runtime when the
property is set from JavaScript code (or if a set on the property is 
performed using a N-API call).
- `method`: Set this if you want the property descriptor object's `value`
property to be a JavaScript function represented by `method`. If this is
passed in, set `value`, `getter` and `setter` to `NULL` (since these members
won't be used)
- `data`: The callback data passed into `method`, `getter` and `setter` if 
this function is invoked
- `attributes`: The attributes associated with the particular property. 
See `napi_property_attributes`

## Functions

### *napi_get_property_names*

#### Signature
```
napi_status napi_get_property_names(napi_env env,
                                    napi_value object,
                                    napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[out] result`: A `napi_value` representing an array of JavaScript values 
that represent the property names of the object. You can iterate over `result`
using `napi_get_array_length` and `napi_get_element`.

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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object on which to set the property
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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to retrieve the property
- `[in]  key`: The name of the property to retrieve
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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object to query
- `[in]  key`: The name of the property whose existence to check
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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object on which to set the property
- `[in]  utf8Name`: The name of the property to set
- `[in]  value`: The property value

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This method is equivalent to calling `napi_set_property` with a `napi_value`
created from the string passed in as `utf8Name`

### *napi_get_named_property*

#### Signature
```
napi_status napi_get_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    napi_value* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to retrieve the property
- `[in]  utf8Name`: The name of the property to get
- `[out] result`: The value of the property

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This method is equivalent to calling `napi_get_property` with a `napi_value`
created from the string passed in as `utf8Name`

### *napi_has_named_property*

#### Signature
```
napi_status napi_has_named_property(napi_env env,
                                    napi_value object,
                                    const char* utf8Name,
                                    bool* result);
```

#### Parameters
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object to query
- `[in]  utf8Name`: The name of the property whose existence to check
- `[out] result`: Whether the property exists on the object or not

#### Return value
- `napi_ok` if the API succeeded.

#### Description
This method is equivalent to calling `napi_has_property` with a `napi_value`
created from the string passed in as `utf8Name`

### *napi_set_element*

#### Signature
```
napi_status napi_set_element(napi_env env,
                             napi_value object,
                             uint32_t index,
                             napi_value value);
```

#### Parameters
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to set the properties
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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to retrieve the property
- `[in]  index`: The index of the property to get
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
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object to query
- `[in]  index`: The index of the property whose existence to check
- `[out] result`: Whether the property exists on the object or not

#### Return value
- `napi_ok` if the API succeeded.

### *napi_define_properties*

#### Signature
```
napi_status napi_define_properties(napi_env env,
                                   napi_value object,
                                   size_t property_count,
                                   const napi_property_descriptor* properties);
```

#### Parameters
- `[in]  env`: The environment that the N-API call is invoked under
- `[in]  object`: The object from which to retrieve the properties
- `[in]  property_count`: The number of elements in the `properties` array
- `[in]  properties`: The array of property descriptors

#### Description
This method allows you to efficiently define multiple properties on a given 
object. The properties are defined using property descriptors (See 
`napi_property_descriptor`). Given an array of such property descriptors, this
API will set the properties on the object one at a time, as defined by
DefineOwnProperty (described in [Section 9.1.6](https://tc39.github.io/ecma262/#sec-ordinary-object-internal-methods-and-internal-slots-defineownproperty-p-desc) 
of the ECMA262 specification)