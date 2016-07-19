#include <node_jsvmapi.h>

void Test(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_symbol) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a symbol.");
    return;
  }

  char buffer[128];
  int buffer_size = 128;

  int remain = napi_get_string_from_value(env, args[0], buffer, buffer_size);

  if (remain == 0) {
    napi_value output = napi_create_string(env, buffer);
    napi_set_return_value(env, info, output);
  }
}

void New(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) >= 1) {
    napi_value args[1];
    napi_get_cb_args(env, info, args, 1);

    if (napi_get_type_of_value(env, args[0]) != napi_string) {
      napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
      return;
    }
    char buffer[128];
    int buffer_size = 128;
    int remain = napi_get_string_from_value(env, args[0], buffer, buffer_size);
    if (remain  == 0) {
      napi_value symbol = napi_create_symbol(env, buffer);
      napi_set_return_value(env, info, symbol);
    }
  }
  else {
    napi_value symbol = napi_create_symbol(env, NULL);
    napi_set_return_value(env, info, symbol);
  }
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "New"),
                    napi_create_function(env, New));
}

NODE_MODULE_ABI(addon, Init)
