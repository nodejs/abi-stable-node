#include "myobject.h"

void CreateObject(napi_env env, napi_callback_info info) {
  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);
  napi_set_return_value(env, info, MyObject::NewInstance(env, args[0]));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  MyObject::Init(env);
  napi_property_descriptor desc = { "exports", CreateObject };
  napi_define_property(env, module, &desc);
}

NODE_MODULE_ABI(addon, Init)
