#include <node_api.h>

napi_value Test(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 10;
  napi_value args[10];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype != napi_function) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a function.");
    return NULL;
  }

  napi_value function = args[0];
  napi_value* argv = args + 1;
  argc = argc - 1;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return NULL;

  napi_value result;
  status = napi_call_function(env, global, function, argc, argv, &result);
  if (status != napi_ok) return NULL;

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value fn;
  status =  napi_create_function(env, NULL, Test, NULL, &fn);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "Test", fn);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
