#include <node_jsvmapi.h>

void Method(napi_env env, napi_callback_info info) {
  napi_status status;
  napi_value world;
  status = napi_create_string(env, "world", &world);
  if (status != napi_ok) return;
  status = napi_set_return_value(env, info, world);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;
  napi_property_descriptor desc = { "hello", Method };
  status = napi_define_property(env, exports, &desc);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)

