#include <node_jsvmapi.h>

void Test(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_number) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a number.");
    return;
  }

  double input = napi_get_number_from_value(env, args[0]);
  napi_value output = napi_create_number(env, input);
  napi_set_return_value(env, info, output);  
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "Test"),
                    napi_create_function(env, Test));
}

NODE_MODULE_ABI(addon, Init)
