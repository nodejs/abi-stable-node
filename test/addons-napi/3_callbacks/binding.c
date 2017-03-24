#include <node_api.h>

napi_value RunCallback(napi_env env, const napi_callback_info info) {
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

  napi_value cb = args[0];

  napi_value argv[1];
  status = napi_create_string_utf8(env, "hello world", -1, argv);
  if (status != napi_ok) return NULL;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return NULL;

  status = napi_call_function(env, global, cb, argc, argv, NULL);
  if (status != napi_ok) return NULL;
  return NULL;
}

napi_value RunCallbackWithRecv(napi_env env, const napi_callback_info info) {
  napi_status status;
  
  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  napi_value cb = args[0];
  napi_value recv = args[1];

  status = napi_call_function(env, recv, cb, 0, NULL, NULL);
  if (status != napi_ok) return NULL;
  return NULL;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc[2] = {
      DECLARE_NAPI_METHOD("RunCallback", RunCallback),
      DECLARE_NAPI_METHOD("RunCallbackWithRecv", RunCallbackWithRecv),
  };
  status = napi_define_properties(env, exports, 2, desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
