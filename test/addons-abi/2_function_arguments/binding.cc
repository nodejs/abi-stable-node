#include <node_jsvmapi.h>

void Add(napi_env env, napi_callback_info info) {
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

  napi_valuetype valuetype1;
  status = napi_get_type_of_value(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype0 != napi_number || valuetype1 != napi_number) {
    napi_throw_type_error(env, "Wrong arguments");
    return;
  }

  double value0;
  status = napi_get_number_from_value(env, args[0], &value0);
  if (status != napi_ok) return;

  double value1;
  status = napi_get_number_from_value(env, args[1], &value1);
  if (status != napi_ok) return;

  napi_value sum;
  status = napi_create_number(env, value0 + value1, &sum);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, sum);
  if (status != napi_ok) return;
}


void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;
  napi_property_descriptor addDescriptor = { "add", Add };
  status = napi_define_property(env, exports, &addDescriptor);
  if (status != napi_ok) return;
}

NODE_MODULE_ABI(addon, Init)
