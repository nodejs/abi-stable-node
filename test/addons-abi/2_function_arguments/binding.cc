#include <node_jsvmapi.h>

void Add(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_number ||
    napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "Wrong arguments");
    return;
  }

  double value = napi_get_number_from_value(env, args[0])
               + napi_get_number_from_value(env, args[1]);
  napi_value num = napi_create_number(env, value);

  napi_set_return_value(env, info, num);
}


void Init(napi_env env, napi_value exports, napi_value module) {
  napi_property_descriptor addDescriptor = { "add", Add };
  napi_define_property(env, exports, &addDescriptor);
}

NODE_MODULE_ABI(addon, Init)
