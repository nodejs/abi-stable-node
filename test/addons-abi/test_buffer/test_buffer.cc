#include <node_jsvmapi.h>

void New(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a number.");
    return;
  }

  char data [1024];
  napi_get_string_from_value(env, args[0], data, 1024);
  int len = napi_get_number_from_value(env, args[1]);

  napi_value buf = napi_buffer_new(env, data, len);
  napi_set_return_value(env, info, buf);
}

void Copy(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a number.");
    return;
  }

  char data [1024];
  napi_get_string_from_value(env, args[0], data, 1024);
  int len = napi_get_number_from_value(env, args[1]);
  napi_value buf = napi_buffer_copy(env, data, len);
  napi_set_return_value(env, info, buf);
}

void Data(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if(!napi_buffer_has_instance(env, args[0])) {
    napi_throw_error(env, "Buffer does not exist");
  }

  char* data = napi_buffer_data(env, args[0]);
  napi_value output = napi_create_string(env, data);
  napi_set_return_value(env, info, output);
}

void Length(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if(!napi_buffer_has_instance(env, args[0])) {
    napi_throw_error(env, "Buffer does not exist");
  }

  int len = napi_buffer_length(env, args[0]);
  napi_value output = napi_create_number(env, len);
  napi_set_return_value(env, info, output);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "New"),
                    napi_create_function(env, New));
  napi_set_property(env, exports,
                    napi_property_name(env, "Copy"),
                    napi_create_function(env, Copy));
  napi_set_property(env, exports,
                    napi_property_name(env, "Data"),
                    napi_create_function(env, Data));
  napi_set_property(env, exports,
                    napi_property_name(env, "Length"),
                    napi_create_function(env, Length));
}

NODE_MODULE_ABI(test_buffer, Init)
