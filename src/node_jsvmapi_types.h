#ifndef NODE_JAVASCRIPT_API_TYPES_H_
#define NODE_JAVASCRIPT_API_TYPES_H_

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__ *napi_env;
typedef struct napi_value__ *napi_value;
typedef struct napi_persistent__ *napi_persistent;
typedef struct napi_propertyname__ *napi_propertyname;
typedef const struct napi_func_cb_info__ *napi_func_cb_info;
typedef void (*napi_callback)(napi_env, napi_func_cb_info);
typedef void napi_destruct(void*);



#endif
