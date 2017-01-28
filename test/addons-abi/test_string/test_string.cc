#include <node_jsvmapi.h>

void Copy(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
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

void Length(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  int length = napi_get_string_length(env, args[0]);
  napi_value output = napi_create_number(env, length);
  napi_set_return_value(env, info, output);
}

void Utf8Length(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  int length = napi_get_string_utf8_length(env, args[0]);
  napi_value output = napi_create_number(env, length);
  napi_set_return_value(env, info, output);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "Copy"),
                    napi_create_function(env, Copy, nullptr));
  napi_set_property(env, exports,
                    napi_property_name(env, "Length"),
                    napi_create_function(env, Length, nullptr));
  napi_set_property(env, exports,
                    napi_property_name(env, "Utf8Length"),
                    napi_create_function(env, Utf8Length, nullptr));
}

NODE_MODULE_ABI(addon, Init)
