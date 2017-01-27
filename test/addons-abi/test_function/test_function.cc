#include <node_jsvmapi.h>

void Test(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[10];
  napi_get_cb_args(env, info, args, 10);

  if (napi_get_type_of_value(env, args[0]) != napi_function) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a function.");
    return;
  }

  napi_value function = args[0];
  int argc = napi_get_cb_args_length(env, info) - 1;
  napi_value* argv = args + 1;

  napi_value output = napi_call_function(env, napi_get_global(env), function, argc, argv);
  napi_set_return_value(env, info, output);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "Test"),
                    napi_create_function(env, Test));
}

NODE_MODULE_ABI(addon, Init)
