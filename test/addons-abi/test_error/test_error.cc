#include <node_jsvmapi.h>

void checkError(napi_env e, napi_callback_info info) {
  napi_status status;
  napi_value jsError;

  status = napi_get_cb_args(e, info, &jsError, 1);
  if (status != napi_ok) return;

  bool r;
  status = napi_is_error(e, jsError, &r);
  if (status != napi_ok) return;

  napi_value result;
  if (r) {
      status = napi_get_true(e, &result);
  } else {
      status = napi_get_false(e, &result);
  }
  if (status != napi_ok) return;

  status = napi_set_return_value(e, info, result);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
    { "checkError", checkError }
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
