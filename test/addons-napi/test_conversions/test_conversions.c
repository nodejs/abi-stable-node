#include <node_api.h>

void ThrowLastError(napi_env env) {
  const napi_extended_error_info* error_info = napi_get_last_error_info();
  if (error_info->error_code != napi_ok) {
    napi_throw_error(env, error_info->error_message);
  }
}

napi_value AsBool(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  bool value;
  status = napi_get_value_bool(env, args[0], &value);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_get_boolean(env, value, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value AsInt32(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  int32_t value;
  status = napi_get_value_int32(env, args[0], &value);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value AsUInt32(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  uint32_t value;
  status = napi_get_value_uint32(env, args[0], &value);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value AsInt64(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  int64_t value;
  status = napi_get_value_int64(env, args[0], &value);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_create_number(env, (double)value, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value AsDouble(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  double value;
  status = napi_get_value_double(env, args[0], &value);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value AsString(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  char value[100];
  status = napi_get_value_string_utf8(env, args[0], value, sizeof(value), NULL);
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_create_string_utf8(env, value, -1, &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value ToBool(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_coerce_to_bool(env, args[0], &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value ToNumber(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_coerce_to_number(env, args[0], &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value ToObject(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_coerce_to_object(env, args[0], &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

napi_value ToString(napi_env env, napi_callback_info info) {
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
  if (status != napi_ok) goto Exit;

  napi_value output;
  status = napi_coerce_to_string(env, args[0], &output);
  if (status != napi_ok) goto Exit;

  return output;

Exit:
  if (status != napi_ok) ThrowLastError(env);
  return NULL;
}

#define DECLARE_NAPI_METHOD(name, func)     \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("asBool", AsBool),
      DECLARE_NAPI_METHOD("asInt32", AsInt32),
      DECLARE_NAPI_METHOD("asUInt32", AsUInt32),
      DECLARE_NAPI_METHOD("asInt64", AsInt64),
      DECLARE_NAPI_METHOD("asDouble", AsDouble),
      DECLARE_NAPI_METHOD("asString", AsString),
      DECLARE_NAPI_METHOD("toBool", ToBool),
      DECLARE_NAPI_METHOD("toNumber", ToNumber),
      DECLARE_NAPI_METHOD("toObject", ToObject),
      DECLARE_NAPI_METHOD("toString", ToString),
  };

  napi_status status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
