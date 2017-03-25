#include <node_api.h>

napi_value Get(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return NULL;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an object as first argument.");
    return NULL;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return NULL;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return NULL;
  }

  napi_value object = args[0];
  napi_value output;
  status = napi_get_property(env, object, args[1], &output);
  if (status != napi_ok) return NULL;

  return output;
}

napi_value Set(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 3;
  napi_value args[3];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 3) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return NULL;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return NULL;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return NULL;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return NULL;
  }

  napi_value object = args[0];
  status = napi_set_property(env, object, args[1], args[2]);
  if (status != napi_ok) return NULL;

  napi_value valuetrue;
  status = napi_get_boolean(env, true, &valuetrue);
  if (status != napi_ok) return NULL;

  return valuetrue;
}

napi_value Has(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 2;
  napi_value args[2];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return NULL;
  }

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return NULL;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an object as first argument.");
    return NULL;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return NULL;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return NULL;
  }

  napi_value obj = args[0];
  bool has_property;
  status = napi_has_property(env, obj, args[1], &has_property);
  if (status != napi_ok) return NULL;

  napi_value ret;
  status = napi_get_boolean(env, has_property, &ret);
  if (status != napi_ok) return NULL;

  return ret;
}

napi_value New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value ret;
  status = napi_create_object(env, &ret);
  if (status != napi_ok) return NULL;

  napi_value num;
  status = napi_create_number(env, 987654321, &num);
  if (status != napi_ok) return NULL;

  status = napi_set_named_property(env, ret, "test_number", num);
  if (status != napi_ok) return NULL;

  napi_value str;
  status = napi_create_string_utf8(env, "test string", -1, &str);
  if (status != napi_ok) return NULL;

  status = napi_set_named_property(env, ret, "test_string", str);
  if (status != napi_ok) return NULL;

  return ret;
}

napi_value Inflate(napi_env env, napi_callback_info info) {
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

  if (valuetype != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an object as first argument.");
    return NULL;
  }

  napi_value obj = args[0];

  napi_value propertynames;
  status = napi_get_propertynames(env, obj, &propertynames);
  if (status != napi_ok) return NULL;

  uint32_t i, length;
  status = napi_get_array_length(env, propertynames, &length);
  if (status != napi_ok) return NULL;

  for (i = 0; i < length; i++) {
    napi_value property_str;
    status = napi_get_element(env, propertynames, i, &property_str);
    if (status != napi_ok) return NULL;

    napi_value value;
    status = napi_get_property(env, obj, property_str, &value);
    if (status != napi_ok) return NULL;

    double double_val;
    status = napi_get_value_double(env, value, &double_val);
    if (status != napi_ok) return NULL;

    status = napi_create_number(env, double_val + 1, &value);
    if (status != napi_ok) return NULL;

    status = napi_set_property(env, obj, property_str, value);
    if (status != napi_ok) return NULL;
  }

  return obj;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("Get", Get),
      DECLARE_NAPI_METHOD("Set", Set),
      DECLARE_NAPI_METHOD("Has", Has),
      DECLARE_NAPI_METHOD("New", New),
      DECLARE_NAPI_METHOD("Inflate", Inflate),
  };

  status = napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
