#include <node_api.h>

napi_value Copy(napi_env env, napi_callback_info info) {
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

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return NULL;
  }

  char buffer[128];
  int buffer_size = 128;

  status =
      napi_get_value_string_utf8(env, args[0], buffer, buffer_size, NULL);
  if (status != napi_ok) return NULL;

  napi_value output;
  status = napi_create_string_utf8(env, buffer, -1, &output);
  if (status != napi_ok) return NULL;

  return output;
}

napi_value Length(napi_env env, napi_callback_info info) {
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

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return NULL;
  }

  size_t length;
  status = napi_get_value_string_length(env, args[0], &length);
  if (status != napi_ok) return NULL;

  napi_value output;
  status = napi_create_number(env, (double)length, &output);
  if (status != napi_ok) return NULL;

  return output;
}

napi_value Utf8Length(napi_env env, napi_callback_info info) {
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

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string.");
    return NULL;
  }

  size_t length;
  status = napi_get_value_string_utf8(env, args[0], NULL, 0, &length);
  if (status != napi_ok) return NULL;

  napi_value output;
  status = napi_create_number(env, (double)length, &output);
  if (status != napi_ok) return NULL;

  return output;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor properties[] = {
      DECLARE_NAPI_METHOD("Copy", Copy),
      DECLARE_NAPI_METHOD("Length", Length),
      DECLARE_NAPI_METHOD("Utf8Length", Utf8Length),
  };

  status = napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
