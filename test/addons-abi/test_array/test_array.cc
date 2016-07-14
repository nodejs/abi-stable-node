#include <node_jsvmapi.h>
#include <string.h>

void Test(napi_env napi_env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(napi_env, info) < 2) {
    napi_throw_error(
          napi_env,
          napi_create_type_error(
              napi_env,
              napi_create_string(napi_env, "Wrong number of arguments")));
    return;
  }

  napi_value args[2];
  napi_get_cb_args(napi_env, info, args, 2);

  if (napi_get_type_of_value(napi_env, args[0]) != napi_object) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects an array as first argument.")));
    return;
  }
  if (napi_get_type_of_value(napi_env, args[1]) != napi_number) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects an integer as second argument.")));
    return;
  }

  napi_value array = args[0];
  int index = napi_get_number_from_value(napi_env, args[1]);
  if (napi_is_array(napi_env, array)) {
    int size = napi_get_array_length(napi_env, array);
    if (index >= size) {
      napi_set_return_value(napi_env, info, 
          napi_create_string(napi_env, "Index out of bound!"));
    }
    else if (index < 0) {
      napi_throw_error(
          napi_env,
          napi_create_error(
              napi_env,
              napi_create_string(napi_env, "Invalid index. Expects a positive integer.")));
    }
    else {
      napi_value ret = napi_get_array_element(napi_env, array, index);
      napi_set_return_value(napi_env, info, ret);
    }
  }
}

void New(napi_env napi_env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(napi_env, info) < 1) {
    napi_throw_error(
          napi_env,
          napi_create_type_error(
              napi_env,
              napi_create_string(napi_env, "Wrong number of arguments")));
    return;
  }

  napi_value args[1];
  napi_get_cb_args(napi_env, info, args, 1);

  if (napi_get_type_of_value(napi_env, args[0]) != napi_object) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects an array as first argument.")));
    return;
  }

  napi_value ret = napi_create_array(napi_env);
  int length = napi_get_array_length(napi_env, args[0]);
  for (int i=0; i<length; i++) {
    napi_set_array_element(napi_env, ret, i,
        napi_get_array_element(napi_env, args[0], i));
  }
  napi_set_return_value(napi_env, info, ret);
}

void Init(napi_env napi_env, napi_value exports, napi_value module) {
  napi_set_property(napi_env, exports,
                    napi_property_name(napi_env, "Test"),
                    napi_create_function(napi_env, Test));

  napi_set_property(napi_env, exports,
                    napi_property_name(napi_env, "New"),
                    napi_create_function(napi_env, New));
}

NODE_MODULE_ABI(addon, Init)
