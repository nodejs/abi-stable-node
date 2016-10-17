#include <node_jsvmapi.h>

void Get(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_string) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string as second argument.");
    return;
  }

  napi_value object = args[0];
  char buffer[128];
  int buffer_size = 128;

  int remain = napi_get_string_from_value(env, args[1], buffer, buffer_size);
  if (remain == 0) {
    napi_propertyname property_name = napi_property_name(env, buffer);
    napi_value output = napi_get_property(env, object, property_name);
    napi_set_return_value(env, info, output);
  }
}

void Has(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_string) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string as second argument.");
    return;
  }

  napi_value obj = args[0];
  char buffer[128];
  int buffer_size = 128;

  int remain = napi_get_string_from_value(env, args[1], buffer, buffer_size);
  if (remain == 0) {
    napi_propertyname property_name = napi_property_name(env, buffer);
    napi_value ret =
        napi_create_boolean(env, napi_has_property(env, obj, property_name));
    napi_set_return_value(env, info, ret);
  }
}

void New(napi_env env, napi_func_cb_info info) {
  napi_value ret = napi_create_object(env);

  napi_propertyname test_number = napi_property_name(env, "test_number");
  napi_set_property(env, ret, test_number, napi_create_number(env, 987654321));
  napi_propertyname test_string = napi_property_name(env, "test_string");
  napi_set_property(env, ret, test_string,
                    napi_create_string(env, "test string"));

  napi_set_return_value(env, info, ret);
}

void Inflate(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_value obj = args[0];

  napi_value propertynames = napi_get_propertynames(env, obj);
  int length = napi_get_array_length(env, propertynames);
  for (int i = 0; i < length; i++) {
    napi_propertyname propertyname =
        reinterpret_cast<napi_propertyname>(
            napi_get_element(env, propertynames, i));
    napi_value value = napi_get_property(env, obj, propertyname);
    double double_val = napi_get_number_from_value(env, value);
    value = napi_create_number(env, double_val + 1);
    napi_set_property(env, obj, propertyname, value);
  }
  napi_set_return_value(env, info, obj);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "Get"),
                    napi_create_function(env, Get));
  napi_set_property(env, exports,
                    napi_property_name(env, "Has"),
                    napi_create_function(env, Has));
  napi_set_property(env, exports,
                    napi_property_name(env, "New"),
                    napi_create_function(env, New));
  napi_set_property(env, exports,
                    napi_property_name(env, "Inflate"),
                    napi_create_function(env, Inflate));
}

NODE_MODULE_ABI(addon, Init)
