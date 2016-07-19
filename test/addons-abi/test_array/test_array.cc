#include <node_jsvmapi.h>
#include <string.h>

void Test(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env, "Wrong type of argments. Expects an array as first argument.");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "Wrong type of argments. Expects an integer as second argument.");
    return;
  }

  napi_value array = args[0];
  int index = napi_get_number_from_value(env, args[1]);
  if (napi_is_array(env, array)) {
    int size = napi_get_array_length(env, array);
    if (index >= size) {
      napi_set_return_value(env, info, 
          napi_create_string(env, "Index out of bound!"));
    }
    else if (index < 0) {
      napi_throw_type_error(env, "Invalid index. Expects a positive integer.");
    }
    else {
      napi_value ret = napi_get_array_element(env, array, index);
      napi_set_return_value(env, info, ret);
    }
  }
}

void New(napi_env env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(env, info) < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env, "Wrong type of argments. Expects an array as first argument.");
    return;
  }

  napi_value ret = napi_create_array(env);
  int length = napi_get_array_length(env, args[0]);
  for (int i=0; i<length; i++) {
    napi_set_array_element(env, ret, i,
        napi_get_array_element(env, args[0], i));
  }
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
