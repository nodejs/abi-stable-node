#include <node_jsvmapi.h>

void Get(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  status = napi_get_cb_args(env, info, args, 2);
  if (status != napi_ok) return;

  napi_valuetype valuetype0;
  status = napi_get_type_of_value(env, args[0], &valuetype0);
  if (status != napi_ok) return;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_get_type_of_value(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_string) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string as second argument.");
    return;
  }

  napi_value object = args[0];
  char buffer[128];
  int buffer_size = 128;

  status = napi_get_value_string_utf8(env, args[1], buffer, buffer_size, nullptr);
  if (status != napi_ok) return;

  napi_propertyname property_name;
  status = napi_property_name(env, buffer, &property_name);
  if (status != napi_ok) return;

  napi_value output;
  status = napi_get_property(env, object, property_name, &output);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) return;
}

void Has(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  status = napi_get_cb_args(env, info, args, 2);
  if (status != napi_ok) return;

  napi_valuetype valuetype0;
  status = napi_get_type_of_value(env, args[0], &valuetype0);
  if (status != napi_ok) return;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_get_type_of_value(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_string) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string as second argument.");
    return;
  }

  napi_value obj = args[0];
  const int buffer_size = 128;
  char buffer[buffer_size];

  status = napi_get_value_string_utf8(env, args[1], buffer, buffer_size, nullptr);
  if (status != napi_ok) return;

  napi_propertyname property_name;
  status = napi_property_name(env, buffer, &property_name);
  if (status != napi_ok) return;

  bool has_property;
  status = napi_has_property(env, obj, property_name, &has_property);
  if (status != napi_ok) return;

  napi_value ret;
  status = napi_create_boolean(env, has_property, &ret);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

void New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value ret;
  status = napi_create_object(env, &ret);

  napi_propertyname test_number;
  status = napi_property_name(env, "test_number", &test_number);
  if (status != napi_ok) return;

  napi_value num;
  status = napi_create_number(env, 987654321, &num);
  if (status != napi_ok) return;

  status = napi_set_property(env, ret, test_number, num);
  if (status != napi_ok) return;

  napi_propertyname test_string;
  status = napi_property_name(env, "test_string", &test_string);
  if (status != napi_ok) return;

  napi_value str;
  status = napi_create_string_utf8(env, "test string", -1, &str);
  if (status != napi_ok) return;

  status = napi_set_property(env, ret, test_string, str);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

void Inflate(napi_env env, napi_callback_info info) {
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

  if (valuetype != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_value obj = args[0];

  napi_value propertynames;
  status = napi_get_propertynames(env, obj, &propertynames);
  if (status != napi_ok) return;

  uint32_t length;
  status = napi_get_array_length(env, propertynames, &length);
  if (status != napi_ok) return;

  for (uint32_t i = 0; i < length; i++) {
    napi_value property_str;
    status = napi_get_element(env, propertynames, i, &property_str);
    if (status != napi_ok) return;

    const int buffer_size = 128;
    char buffer[buffer_size];

    status = napi_get_value_string_utf8(env, property_str, buffer, buffer_size, nullptr);
    if (status != napi_ok) return;

    napi_propertyname propertyname;
    status = napi_property_name(env, buffer, &propertyname);
    if (status != napi_ok) return;

    napi_value value;
    status = napi_get_property(env, obj, propertyname, &value);
    if (status != napi_ok) return;

    double double_val;
    status = napi_get_value_double(env, value, &double_val);
    if (status != napi_ok) return;

    status = napi_create_number(env, double_val + 1, &value);
    if (status != napi_ok) return;

    status = napi_set_property(env, obj, propertyname, value);
    if (status != napi_ok) return;
  }
  status = napi_set_return_value(env, info, obj);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
    { "Get", Get },
    { "Has", Has },
    { "New", New },
    { "Inflate", Inflate },
  };

  for (int i = 0; i < sizeof(descriptors) / sizeof(*descriptors); i++) {
    status = napi_define_property(env, exports, &descriptors[i]);
    if (status != napi_ok) return;
  }
}

NODE_MODULE_ABI(addon, Init)
