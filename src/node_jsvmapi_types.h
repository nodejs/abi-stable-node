#ifndef SRC_NODE_JSVMAPI_TYPES_H_
#define SRC_NODE_JSVMAPI_TYPES_H_

#define nullptr NULL

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__ *napi_env;
typedef struct napi_value__ *napi_value;
typedef struct napi_persistent__ *napi_persistent;
typedef struct napi_handle_scope__ *napi_handle_scope;
typedef struct napi_escapable_handle_scope__ *napi_escapable_handle_scope;
typedef struct napi_propertyname__ *napi_propertyname;
typedef const struct napi_func_cb_info__ *napi_func_cb_info;
typedef void (*napi_callback)(napi_env, napi_func_cb_info);
typedef void napi_destruct(void* v);


struct napi_method_descriptor {
  napi_callback callback;
  char* utf8name;
};



#endif  // SRC_NODE_JSVMAPI_TYPES_H_
