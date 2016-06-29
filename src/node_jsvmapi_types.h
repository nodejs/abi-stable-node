#ifndef NODE_JAVASCRIPT_API_TYPES_H_
#define NODE_JAVASCRIPT_API_TYPES_H_

// JSVM API types are all opaque pointers for ABI stability
typedef void* napi_env;
typedef void* napi_value;
typedef void* napi_persistent;
typedef napi_value napi_propertyname;
typedef const void* napi_func_cb_info;
typedef void (*napi_callback)(napi_env, napi_func_cb_info);
typedef void napi_destruct(void*);



#endif
