#include <node_jsvmapi.h>

void RunCallback(napi_env env, const napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_value cb = args[0];

  napi_value argv[1];
  status = napi_create_string_utf8(env, "hello world", -1, argv);
  if (status != napi_ok) return;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return;

  status = napi_call_function(env, global, cb, 1, argv, nullptr);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;
  napi_property_descriptor desc = { "exports", RunCallback };
  status = napi_define_property(env, module, &desc);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
