#include <node_jsvmapi.h>
#include <node_api_helpers.h>

static bool exceptionWasPending = false;

NAPI_METHOD(returnException) {
  napi_status status;
  napi_value jsFunction;

  status = napi_get_cb_args(env, info, &jsFunction, 1);
  if (status != napi_ok) return;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_call_function(env, global, jsFunction, 0, 0, &result);
  if (status ==  napi_pending_exception) {
    napi_value ex;
    status = napi_get_and_clear_last_exception(env, &ex);
    if (status != napi_ok) return;

    status = napi_set_return_value(env, info, ex);
    if (status != napi_ok) return;
  }
}

NAPI_METHOD(allowException) {
  napi_status status;
  napi_value jsFunction;

  status = napi_get_cb_args(env, info, &jsFunction, 1);
  if (status != napi_ok) return;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_call_function(env, global, jsFunction, 0, 0, &result);
  // Ignore status and check napi_is_exception_pending() instead.

  status = napi_is_exception_pending(env, &exceptionWasPending);
  if (status != napi_ok) return;
}

NAPI_METHOD(wasPending) {
  napi_status status;

  napi_value result;
  status = napi_create_boolean(env, exceptionWasPending, &result);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

NAPI_MODULE_INIT(Init) {
  napi_status status;
  napi_propertyname name;
  napi_value fn;

  status = napi_property_name(env, "returnException", &name);
  if (status != napi_ok) return;
  status = napi_create_function(env, returnException, nullptr, &fn);
  if (status != napi_ok) return;
  status = napi_set_property(env, exports, name, fn);
  if (status != napi_ok) return;

  status = napi_property_name(env, "allowException", &name);
  if (status != napi_ok) return;
  status = napi_create_function(env, allowException, nullptr, &fn);
  if (status != napi_ok) return;
  status = napi_set_property(env, exports, name, fn);
  if (status != napi_ok) return;

  status = napi_property_name(env, "wasPending", &name);
  if (status != napi_ok) return;
  status = napi_create_function(env, wasPending, nullptr, &fn);
  if (status != napi_ok) return;
  status = napi_set_property(env, exports, name, fn);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
