# Module registration

N-API based modules are registered the same manner as other module
except that instead of using the NODE_MODULE macro you use:

```C
NAPI_MODULE(addon, Init)
```

The next difference is that the signature for the 'Init'.  For a N-API
based module it is as follows:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv);
```

As with any other module, functions are exported by either adding them to
the exports or module objects passed to the Init method.

For example, to add the method 'hello' as a function so that can be called as a
method provided by the addon:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc =
    {"hello", Method, 0, 0, 0, napi_default, 0};
  status = napi_define_properties(env, exports, 1, &desc);
}
```

For example, to set a function to be returned by the require for the addon:

```C
void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc =
    {"exports", Method, 0, 0, 0, napi_default, 0};
  status = napi_define_properties(env, module, 1, &desc);
}
```

For more details on setting properties on either the exports or module objects,
see the secton on setting, getting and defining properties.

For more details on building Addon modules in general, refer to the existing API
documentation in [addons.md](addons.md).  Most of that informatoin remains
accurate when building N-API modules,  except for references to the use
of V8 functions which should be replaced by use of N-API methods. Once N-API
is no longer experimental we'll look to more completely unify
the documentation for Addons.
