#include <node_api.h>

napi_value Add(napi_env env, napi_callback_info info) {
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

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return NULL;

  if (valuetype0 != napi_number || valuetype1 != napi_number) {
    napi_throw_type_error(env, "Wrong arguments");
    return NULL;
  }

  double value0;
  status = napi_get_value_double(env, args[0], &value0);
  if (status != napi_ok) return NULL;

  double value1;
  status = napi_get_value_double(env, args[1], &value1);
  if (status != napi_ok) return NULL;

  napi_value sum;
  status = napi_create_number(env, value0 + value1, &sum);
  if (status != napi_ok) return NULL;

  return sum;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor addDescriptor = DECLARE_NAPI_METHOD("add", Add);
  status = napi_define_properties(env, exports, 1, &addDescriptor);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
