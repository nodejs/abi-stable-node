#include <node_jsvmapi.h>

void Test(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_get_type_of_value(env, args[0], &valuetype);
  if (status != napi_ok) return;

  if (valuetype != napi_number) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a number.");
    return;
  }

  double input;
  status = napi_get_number_from_value(env, args[0], &input);
  if (status != napi_ok) return;

  napi_value output;
  status = napi_create_number(env, input, &output);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
    { "Test", Test },
  };

  for (int i = 0; i < sizeof(descriptors) / sizeof(*descriptors); i++) {
    status = napi_define_property(env, exports, &descriptors[i]);
    if (status != napi_ok) return;
  }
}

NODE_MODULE_ABI(addon, Init)
