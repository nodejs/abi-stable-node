#include <node_jsvmapi.h>

void MyFunction(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value str;
  status = napi_create_string_utf8(env, "hello world", -1, &str);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, str);
  if (status != napi_ok) return;
}

void CreateFunction(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value fn;
  status = napi_create_function(env, "theFunction", MyFunction, nullptr, &fn);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, fn);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;
  napi_property_descriptor desc = { "exports", CreateFunction, nullptr, nullptr, nullptr, napi_default, nullptr };
  status = napi_define_properties(env, module, 1, &desc);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)

