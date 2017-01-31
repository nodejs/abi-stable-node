#include <node_jsvmapi.h>

void Copy(napi_env env, napi_callback_info info) {
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

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  char buffer[128];
  int buffer_size = 128;

  int remain;
  status = napi_get_string_from_value(env, args[0], buffer, buffer_size, &remain);
  if (status != napi_ok) return;

  if (remain == 0) {
    napi_value output;
    status = napi_create_string(env, buffer, &output);
    if (status != napi_ok) return;

    status = napi_set_return_value(env, info, output);
    if (status != napi_ok) return;
  }
}

void Length(napi_env env, napi_callback_info info) {
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

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  int length;
  status = napi_get_string_length(env, args[0], &length);
  if (status != napi_ok) return;

  napi_value output;
  status = napi_create_number(env, length, &output);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) return;
}

void Utf8Length(napi_env env, napi_callback_info info) {
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

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return;
  }

  int length;
  status = napi_get_string_utf8_length(env, args[0], &length);
  if (status != napi_ok) return;

  napi_value output;
  status = napi_create_number(env, length, &output);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor properties[] = {
    { "Copy", Copy },
    { "Length", Length },
    { "Utf8Length", Utf8Length },
  };

  for (int i = 0; i < sizeof(properties) / sizeof(*properties); i++) {
    status = napi_define_property(env, exports, &properties[i]);
    if (status != napi_ok) return;
  }
}

NODE_MODULE_ABI(addon, Init)
