#include <node_jsvmapi.h>

void Method(napi_env env, napi_callback_info info) {
  napi_set_return_value(
        env,
        info,
        napi_create_string(env, "world"));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_property_descriptor desc = { "hello", Method };
  napi_define_property(env, exports, &desc);
}

NODE_MODULE_ABI(addon, Init)

