#include <node_jsvmapi.h>
#include <stdio.h>

void doInstanceOf(napi_env env, napi_callback_info info) {
  napi_value arguments[2];

  napi_get_cb_args(env, info, arguments, 2);

  napi_set_return_value(env, info,
    napi_create_boolean(env, napi_instanceof(env, arguments[0], arguments[1])));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "doInstanceOf"),
                    napi_create_function(env, doInstanceOf, nullptr));
}

NODE_MODULE_ABI(addon, Init)
