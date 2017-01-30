#include "myobject.h"

MyObject::MyObject() {}
MyObject::~MyObject() {}

void MyObject::Destructor(void* nativeObject) {
  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
}

napi_persistent MyObject::constructor;

napi_status MyObject::Init(napi_env env) {
  napi_status status;
  napi_property_descriptor properties[] = {
    { "plusOne", PlusOne },
  };

  napi_value cons;
  status = napi_create_constructor(env, "MyObject", New, nullptr, 1, properties, &cons);
  if (status != napi_ok) return status;

  status = napi_create_persistent(env, cons, &constructor);
  if (status != napi_ok) return status;

  return napi_ok;
}

void MyObject::New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_get_type_of_value(env, args[0], &valuetype);
  if (status != napi_ok) return;

  MyObject* obj = new MyObject();

  if (valuetype == napi_undefined) {
    obj->counter_ = 0;
  }
  else {
    status = napi_get_number_from_value(env, args[0], &obj->counter_);
    if (status != napi_ok) return;
  }

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  status = napi_wrap(env, jsthis, reinterpret_cast<void*>(obj),
                     MyObject::Destructor, nullptr);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, jsthis);
  if (status != napi_ok) return;
}


napi_status MyObject::NewInstance(napi_env env, napi_value arg, napi_value* instance) {
  napi_status status;

  const int argc = 1;
  napi_value argv[argc] = { arg };

  napi_value cons;
  status = napi_get_persistent_value(env, constructor, &cons);
  if (status != napi_ok) return status;

  status = napi_new_instance(env, cons, argc, argv, instance);
  if (status != napi_ok) return status;

  return napi_ok;
}

void MyObject::PlusOne(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  obj->counter_ += 1;

  napi_value num;
  status = napi_create_number(env, obj->counter_, &num);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, num);
  if (status != napi_ok) return;
}
