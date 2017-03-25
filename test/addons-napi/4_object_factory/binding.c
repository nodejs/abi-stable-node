#include <node_api.h>

napi_value CreateObject(napi_env env, const napi_callback_info info) {
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

  napi_value obj;
  status = napi_create_object(env, &obj);
  if (status != napi_ok) return NULL;

  status = napi_set_named_property(env, obj, "msg", args[0]);
  if (status != napi_ok) return NULL;
  
  return obj;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc = DECLARE_NAPI_METHOD("exports", CreateObject);
  status = napi_define_properties(env, module, 1, &desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
