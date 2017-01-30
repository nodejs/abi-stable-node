#include <node_jsvmapi.h>

static double value_ = 1;

void GetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (napi_get_cb_args_length(env, info) != 0) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value number = napi_create_number(env, value_);
  napi_set_return_value(env, info, number);
}

void SetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (napi_get_cb_args_length(env, info) != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  napi_get_cb_args(env, info, &arg, 1);

  value_ = napi_get_number_from_value(env, arg);
}

void Echo(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (napi_get_cb_args_length(env, info) != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  napi_get_cb_args(env, info, &arg, 1);

  napi_set_return_value(env, info, arg);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_value number = napi_create_number(env, value_);
  napi_property_descriptor properties[] = {
    { "echo", Echo },
    { "accessorValue", nullptr, GetValue, SetValue },
    { "readwriteValue", nullptr, nullptr, nullptr, number },
    { "readonlyValue", nullptr, nullptr, nullptr, number, napi_read_only },
    { "hiddenValue", nullptr, nullptr, nullptr, number,
      static_cast<napi_property_attributes>(napi_read_only | napi_dont_enum) },
  };

  for (int i = 0; i < sizeof(properties) / sizeof(*properties); i++) {
    napi_define_property(env, exports, &properties[i]);
  }
}

NODE_MODULE_ABI(addon, Init)
