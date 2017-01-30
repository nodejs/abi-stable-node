#include <node_jsvmapi.h>
#include <stdio.h>

void doInstanceOf(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value arguments[2];

  status = napi_get_cb_args(env, info, arguments, 2);
  if (status != napi_ok) return;

  bool instanceof;
  status = napi_instanceof(env, arguments[0], arguments[1], &instanceof);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_create_boolean(env, instanceof, &result);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_propertyname name;
  status = napi_property_name(env, "doInstanceOf", &name);
  if (status != napi_ok) return;

  napi_value fn;
  status = napi_create_function(env, doInstanceOf, nullptr, &fn);
  if (status != napi_ok) return;

  status = napi_set_property(env, exports, name, fn);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
