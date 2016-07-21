#include <node_jsvmapi.h>

void Test(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env, "Wrong type of argments. Expects an object as first argument.");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_string) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a string as second argument.");
    return;
  }

  napi_value object = args[0];
  char buffer [128];
  int buffer_size = 128;

  int remain = napi_get_string_from_value(env, args[1], buffer, buffer_size);
  if (remain == 0) {
    napi_propertyname property_name = napi_property_name(env, buffer);
    napi_value output = napi_get_property(env, object, property_name);
    napi_set_return_value(env, info, output);
  }
}

void New(napi_env env, napi_func_cb_info info) {
  napi_value ret = napi_create_object(env);

  napi_propertyname test_number = napi_property_name(env, "test_number");
  napi_set_property(env, ret, test_number, napi_create_number(env, 987654321));
  napi_propertyname test_string = napi_property_name(env, "test_string");
  napi_set_property(env, ret, test_string, napi_create_string(env, "test string"));
 
  napi_set_return_value(env, info, ret);
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_set_property(env, exports,
                    napi_property_name(env, "Test"),
                    napi_create_function(env, Test));
  napi_set_property(env, exports,
                    napi_property_name(env, "New"),
                    napi_create_function(env, New));

}

NODE_MODULE_ABI(addon, Init)
