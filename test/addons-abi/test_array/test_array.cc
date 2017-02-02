#include <node_jsvmapi.h>
#include <string.h>

void Test(napi_env env, napi_callback_info info) {
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
        "Wrong type of argments. Expects an array as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_get_type_of_value(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_number) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an integer as second argument.");
    return;
  }

  napi_value array = args[0];
  int index;
  status = napi_get_value_int32(env, args[1], &index);
  if (status != napi_ok) return;

  bool isarray;
  status = napi_is_array(env, array, &isarray);
  if (status != napi_ok) return;

  if (isarray) {
    uint32_t size;
    status = napi_get_array_length(env, array, &size);
    if (status != napi_ok) return;

    if (index >= static_cast<int>(size)) {
      napi_value str;
      status = napi_create_string_utf8(env, "Index out of bound!", -1, &str);
      if (status != napi_ok) return;

      status = napi_set_return_value(env, info, str);
      if (status != napi_ok) return;
    }
    else if (index < 0) {
      napi_throw_type_error(env, "Invalid index. Expects a positive integer.");
    }
    else {
      napi_value ret;
      status = napi_get_element(env, array, index, &ret);
      if (status != napi_ok) return;

      status = napi_set_return_value(env, info, ret);
      if (status != napi_ok) return;
    }
  }
}

void New(napi_env env, napi_callback_info info) {
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
        "Wrong type of argments. Expects an array as first argument.");
    return;
  }

  napi_value ret;
  status = napi_create_array(env, &ret);
  if (status != napi_ok) return;

  uint32_t length;
  status = napi_get_array_length(env, args[0], &length);
  if (status != napi_ok) return;

  for (uint32_t i = 0; i < length; i++) {
    napi_value e;
    status = napi_get_element(env, args[0], i, &e);
    if (status != napi_ok) return;

    status = napi_set_element(env, ret, i, e);
    if (status != napi_ok) return;
  }

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
    { "Test", Test },
    { "New", New },
  };

  for (int i = 0; i < sizeof(descriptors) / sizeof(*descriptors); i++) {
    status = napi_define_property(env, exports, &descriptors[i]);
    if (status != napi_ok) return;
  }
}

NODE_MODULE_ABI(addon, Init)
