#include <node_api.h>

static double value_ = 1;

napi_value GetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 0;
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    NULL,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc != 0) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_value number;
  status = napi_create_number(env, value_, &number);
  if (status != napi_ok) return NULL;

  return number;
}

napi_value SetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  status = napi_get_value_double(env, args[0], &value_);
  if (status != napi_ok) return NULL;
  return NULL;
}

napi_value Echo(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  return args[0];
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value number;
  status = napi_create_number(env, value_, &number);
  if (status != napi_ok) return;

  napi_property_descriptor properties[] = {
      { "echo", 0, Echo, 0, 0, 0, napi_default, 0 },
      { "accessorValue", 0, 0, GetValue, SetValue, 0, napi_default, 0 },
      { "readwriteValue", 0, 0, 0, 0, number, napi_default, 0 },
      { "readonlyValue", 0, 0, 0, 0, number, napi_read_only, 0 },
      { "hiddenValue", 0, 0, 0, 0, number, napi_read_only | napi_dont_enum, 0 },
  };

  status = napi_define_properties(
    env, exports, sizeof(properties) / sizeof(*properties), properties);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
