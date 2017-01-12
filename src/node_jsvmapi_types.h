#ifndef SRC_NODE_JSVMAPI_TYPES_H_
#define SRC_NODE_JSVMAPI_TYPES_H_

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__ *napi_env;
typedef struct napi_value__ *napi_value;
typedef struct napi_persistent__ *napi_persistent;
typedef struct napi_weakref__ *napi_weakref;
typedef struct napi_handle_scope__ *napi_handle_scope;
typedef struct napi_escapable_handle_scope__ *napi_escapable_handle_scope;
typedef struct napi_propertyname__ *napi_propertyname;
typedef struct napi_callback_info__ *napi_callback_info;

typedef void (*napi_callback)(napi_env, napi_callback_info);
typedef void (*napi_destruct)(void* v);

enum napi_property_attributes {
  napi_default = 0,
  napi_read_only = 1 << 0,
  napi_dont_enum = 1 << 1,
  napi_dont_delete = 1 << 2
};

struct napi_property_descriptor {
  const char* utf8name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
};

#endif  // SRC_NODE_JSVMAPI_TYPES_H_
