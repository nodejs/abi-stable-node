#include <node_jsvmapi.h>

void MyFunction(napi_env env, napi_callback_info info) {
  napi_set_return_value(env, info, napi_create_string(env, "hello world"));
}

void CreateFunction(napi_env env, napi_callback_info info) {
  napi_value fn = napi_create_function(env, MyFunction, nullptr);

  // omit this to make it anonymous
  napi_set_function_name(env, fn, napi_property_name(env, "theFunction"));

  napi_set_return_value(env, info, fn);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_property_descriptor desc = { "exports", CreateFunction };
  napi_define_property(env, module, &desc);
}

NODE_MODULE_ABI(addon, Init)

