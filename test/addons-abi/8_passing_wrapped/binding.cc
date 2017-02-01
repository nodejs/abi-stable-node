#include "myobject.h"

void CreateObject(napi_env env, napi_callback_info info) {
  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);
  napi_set_return_value(env, info, MyObject::NewInstance(env, args[0]));
}

void Add(napi_env env, napi_callback_info info) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);
  MyObject* obj1 = reinterpret_cast<MyObject*>(napi_unwrap(env, args[0]));
  MyObject* obj2 = reinterpret_cast<MyObject*>(napi_unwrap(env, args[1]));
  double sum = obj1->Val() + obj2->Val();
  napi_set_return_value(env, info, napi_create_number(env, sum));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  MyObject::Init(env);

  napi_property_descriptor desc = { "createObject", CreateObject };
  napi_define_property(env, exports, &desc);

  napi_property_descriptor desc2 = { "add", Add };
  napi_define_property(env, exports, &desc2);
}


NODE_MODULE_ABI(addon, Init)
