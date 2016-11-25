#include <node_jsvmapi.h>
#include <node_api_helpers.h>

NAPI_METHOD(doInstanceOf) {
  napi_value arguments[2];

  napi_get_cb_args(env, info, arguments, 2);

  napi_set_return_value(env, info,
    napi_create_boolean(env, napi_instanceof(env, arguments[0], arguments[1])));
}

NAPI_MODULE_INIT(Init) {
  napi_set_property(env, exports,
                    napi_property_name(env, "doInstanceOf"),
                    napi_create_function(env, doInstanceOf));
}

NODE_MODULE_ABI(addon, Init)
