#include <node_api.h>

static bool exceptionWasPending = false;

napi_value returnException(napi_env env, napi_callback_info info) {
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

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return NULL;

  napi_value result;
  status = napi_call_function(env, global, args[0], 0, 0, &result);
  if (status ==  napi_pending_exception) {
    napi_value ex;
    status = napi_get_and_clear_last_exception(env, &ex);
    if (status != napi_ok) return NULL;

    return ex;
  }

  return NULL;
}

napi_value allowException(napi_env env, napi_callback_info info) {
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

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return NULL;

  napi_value result;
  status = napi_call_function(env, global, args[0], 0, 0, &result);
  // Ignore status and check napi_is_exception_pending() instead.

  status = napi_is_exception_pending(env, &exceptionWasPending);
  if (status != napi_ok) return NULL;
  return NULL;
}

napi_value wasPending(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value result;
  status = napi_get_boolean(env, exceptionWasPending, &result);
  if (status != napi_ok) return NULL;

  return result;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("returnException", returnException),
      DECLARE_NAPI_METHOD("allowException", allowException),
      DECLARE_NAPI_METHOD("wasPending", wasPending),
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
