#include "myobject.h"

MyObject::MyObject() : env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { napi_delete_reference(env_, wrapper_); }

void MyObject::Destructor(void* nativeObject, void* /*finalize_hint*/) {
  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
}

napi_ref MyObject::constructor;

napi_status MyObject::Init(napi_env env) {
  napi_status status;

  napi_value cons;
  status = napi_define_class(env, "MyObject", New, nullptr, 0, nullptr, &cons);
  if (status != napi_ok) return status;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return status;

  return napi_ok;
}

napi_value MyObject::New(napi_env env, napi_callback_info info) {
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

  MyObject* obj = new MyObject();

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return NULL;

  if (valuetype == napi_undefined) {
    obj->val_ = 0;
  } else {
    status = napi_get_value_double(env, args[0], &obj->val_);
    if (status != napi_ok) return NULL;
  }

  obj->env_ = env;
  status = napi_wrap(env,
                     _this,
                     reinterpret_cast<void*>(obj),
                     MyObject::Destructor,
                     nullptr,  // finalize_hint
                     &obj->wrapper_);
  if (status != napi_ok) return NULL;

  return _this;
}

napi_status MyObject::NewInstance(napi_env env,
                                  napi_value arg,
                                  napi_value* instance) {
  napi_status status;

  const int argc = 1;
  napi_value argv[argc] = {arg};

  napi_value cons;
  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) return status;

  status = napi_new_instance(env, cons, argc, argv, instance);
  if (status != napi_ok) return status;

  return napi_ok;
}
