#include <node_api.h>

napi_value checkError(napi_env env, napi_callback_info info) {
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

  bool r;
  status = napi_is_error(env, args[0], &r);
  if (status != napi_ok) return NULL;

  napi_value result;
  status = napi_get_boolean(env, r, &result);
  if (status != napi_ok) return NULL;

  return result;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("checkError", checkError),
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
