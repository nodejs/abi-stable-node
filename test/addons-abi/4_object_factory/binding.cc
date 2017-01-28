#include <node_jsvmapi.h>

void CreateObject(napi_env env, const napi_callback_info info) {
  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  napi_value obj = napi_create_object(env);
  napi_set_property(env, obj, napi_property_name(env, "msg"),
                        args[0]);

  napi_set_return_value(env, info, obj);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_property_descriptor desc = { "exports", CreateObject };
  napi_define_property(env, module, &desc);
}

NODE_MODULE_ABI(addon, Init)
