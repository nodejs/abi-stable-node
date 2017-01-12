#include <node_jsvmapi.h>

static double value_ = 1;
napi_persistent constructor_;

void GetValue(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) != 0) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value number = napi_create_number(env, value_);
  napi_set_return_value(env, info, number);
}

void SetValue(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  napi_get_cb_args(env, info, &arg, 1);

  value_ = napi_get_number_from_value(env, arg);
}

void Echo(napi_env env, napi_callback_info info) {
  if (napi_get_cb_args_length(env, info) != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  napi_get_cb_args(env, info, &arg, 1);

  napi_set_return_value(env, info, arg);
}

void New(napi_env env, napi_callback_info info) {
  napi_value jsthis = napi_get_cb_this(env, info);
  napi_set_return_value(env, info, jsthis);
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
  napi_value cons = napi_create_constructor(env, "MyObject", New,
    nullptr, sizeof(properties)/sizeof(*properties), properties);

  napi_set_property(env, module, napi_property_name(env, "exports"),
                    cons);
  constructor_ = napi_create_persistent(env, cons);
}

NODE_MODULE_ABI(addon, Init)
