#include "myobject.h"

napi_value CreateObject(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  napi_value _this;
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    &_this,
    NULL);
  if (status != napi_ok) return NULL;

  napi_value instance;
  status = MyObject::NewInstance(env, args[0], &instance);
  if (status != napi_ok) return NULL;

  return instance;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  status = MyObject::Init(env);
  if (status != napi_ok) return;

  napi_property_descriptor desc = DECLARE_NAPI_METHOD("exports", CreateObject);
  status = napi_define_properties(env, module, 1, &desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
