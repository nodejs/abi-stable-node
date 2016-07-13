#include <node_jsvmapi.h>

void Test(napi_env napi_env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(napi_env, info) < 2) {
    napi_throw_error(
          napi_env,
          napi_create_type_error(
              napi_env,
              napi_create_string(napi_env, "Wrong number of arguments")));
    return;
  }

  napi_value args[2];
  napi_get_cb_args(napi_env, info, args, 2);

  if (napi_get_type_of_value(napi_env, args[0]) != napi_object) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects an object as first argument.")));
    return;
  }
  if (napi_get_type_of_value(napi_env, args[1]) != napi_string) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects a string as second argument.")));
    return;
  }

  napi_value object = args[0];
  char buffer [128];
  int buffer_size = 128;
  int remain = napi_get_string_from_value(napi_env, args[1], buffer, buffer_size);
  if (remain == 0) {
    napi_propertyname property_name = napi_property_name(napi_env, buffer);
    napi_value output = napi_get_property(napi_env, object, property_name);
    napi_set_return_value(napi_env, info, output);
  }
}

void Init(napi_env napi_env, napi_value exports, napi_value module) {
  napi_set_property(napi_env, exports,
                    napi_property_name(napi_env, "Test"),
                    napi_create_function(napi_env, Test));
}

NODE_MODULE_ABI(addon, Init)
