#include <node_api.h>
#include <string.h>

napi_value Test(napi_env env, napi_callback_info info) {
  napi_status status;
  
  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return NULL;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an array as first argument.");
    return NULL;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return NULL;

  if (valuetype1 != napi_number) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an integer as second argument.");
    return NULL;
  }

  napi_value array = args[0];
  int index;
  status = napi_get_value_int32(env, args[1], &index);
  if (status != napi_ok) return NULL;

  bool isarray;
  status = napi_is_array(env, array, &isarray);
  if (status != napi_ok) return NULL;

  if (isarray) {
    uint32_t size;
    status = napi_get_array_length(env, array, &size);
    if (status != napi_ok) return NULL;

    if (index >= (int)(size)) {
      napi_value str;
      status = napi_create_string_utf8(env, "Index out of bound!", -1, &str);
      if (status != napi_ok) return NULL;

      return str;
    } else if (index < 0) {
      napi_throw_type_error(env, "Invalid index. Expects a positive integer.");
    } else {
      napi_value ret;
      status = napi_get_element(env, array, index, &ret);
      if (status != napi_ok) return NULL;

      return ret;
    }
  }
  
  return NULL;
}

napi_value New(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an array as first argument.");
    return NULL;
  }

  napi_value ret;
  status = napi_create_array(env, &ret);
  if (status != napi_ok) return NULL;

  uint32_t i, length;
  status = napi_get_array_length(env, args[0], &length);
  if (status != napi_ok) return NULL;

  for (i = 0; i < length; i++) {
    napi_value e;
    status = napi_get_element(env, args[0], i, &e);
    if (status != napi_ok) return NULL;

    status = napi_set_element(env, ret, i, e);
    if (status != napi_ok) return NULL;
  }

  return ret;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("Test", Test),
      DECLARE_NAPI_METHOD("New", New),
  };

  status = napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
