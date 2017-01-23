#include "myobject.h"

MyObject::MyObject() {}
MyObject::~MyObject() {}

void MyObject::Destructor(void* nativeObject) {
  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
}

napi_persistent MyObject::constructor;

void MyObject::Init(napi_env env) {
  napi_property_descriptor properties[] = {
    { "plusOne", PlusOne },
  };
  napi_value cons = napi_create_constructor(env, "MyObject", New,
                                            nullptr, 1, properties);
  constructor = napi_create_persistent(env, cons);
}

void MyObject::New(napi_env env, napi_callback_info info) {
  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);
  MyObject* obj = new MyObject();
  obj->counter_ = (args[0] == napi_get_undefined_(env)) ?
                   0 : napi_get_number_from_value(env, args[0]);
  napi_value jsthis = napi_get_cb_this(env, info);
  napi_wrap(env, jsthis, reinterpret_cast<void*>(obj),
            MyObject::Destructor, nullptr);
  napi_set_return_value(env, info, jsthis);
}


napi_value MyObject::NewInstance(napi_env env, napi_value arg) {
  const int argc = 1;
  napi_value argv[argc] = { arg };
  napi_value cons = napi_get_persistent_value(env, constructor);
  return napi_new_instance(env, cons, argc, argv);
}

void MyObject::PlusOne(napi_env env, napi_callback_info info) {
  MyObject* obj = reinterpret_cast<MyObject*>(
                      napi_unwrap(env, napi_get_cb_this(env, info)));
  obj->counter_ += 1;

  napi_set_return_value(env, info, napi_create_number(env, obj->counter_));
}
