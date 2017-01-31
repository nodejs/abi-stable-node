#include <node_jsvmapi.h>

void CreateObject(napi_env env, const napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_value obj;
  status = napi_create_object(env, &obj);
  if (status != napi_ok) return;

  napi_propertyname msgprop;
  status = napi_property_name(env, "msg", &msgprop);
  if (status != napi_ok) return;

  status = napi_set_property(env, obj, msgprop, args[0]);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, obj);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;
  napi_property_descriptor desc = { "exports", CreateObject };
  status = napi_define_property(env, module, &desc);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
