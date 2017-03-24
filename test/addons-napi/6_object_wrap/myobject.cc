#include "myobject.h"

napi_ref MyObject::constructor;

MyObject::MyObject(double value)
    : value_(value), env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { napi_delete_reference(env_, wrapper_); }

void MyObject::Destructor(void* nativeObject, void* /*finalize_hint*/) {
  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void MyObject::Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor properties[] = {
      { "value", nullptr, nullptr, GetValue, SetValue, 0, napi_default, 0 },
      DECLARE_NAPI_METHOD("plusOne", PlusOne),
      DECLARE_NAPI_METHOD("multiply", Multiply),
  };

  napi_value cons;
  status =
      napi_define_class(env, "MyObject", New, nullptr, 3, properties, &cons);
  if (status != napi_ok) return;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "MyObject", cons);
  if (status != napi_ok) return;
}

napi_value MyObject::New(napi_env env, napi_callback_info info) {
  napi_status status;

  bool is_constructor;
  status = napi_is_construct_call(env, info, &is_constructor);
  if (status != napi_ok) return NULL;

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

  if (is_constructor) {
    // Invoked as constructor: `new MyObject(...)`
    double value = 0;

    napi_valuetype valuetype;
    status = napi_typeof(env, args[0], &valuetype);
    if (status != napi_ok) return NULL;

    if (valuetype != napi_undefined) {
      status = napi_get_value_double(env, args[0], &value);
      if (status != napi_ok) return NULL;
    }

    MyObject* obj = new MyObject(value);

    obj->env_ = env;
    status = napi_wrap(env,
                       _this,
                       reinterpret_cast<void*>(obj),
                       MyObject::Destructor,
                       nullptr,  // finalize_hint
                       &obj->wrapper_);
    if (status != napi_ok) return NULL;

    return _this;
  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call.
    argc = 1;
    napi_value argv[1] = {args[0]};

    napi_value cons;
    status = napi_get_reference_value(env, constructor, &cons);
    if (status != napi_ok) return NULL;

    napi_value instance;
    status = napi_new_instance(env, cons, argc, argv, &instance);
    if (status != napi_ok) return NULL;

    return instance;
  }

  return NULL;
}

napi_value MyObject::GetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 0;
  napi_value _this;
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    NULL,
    &_this,
    NULL);
  if (status != napi_ok) return NULL;

  MyObject* obj;
  status = napi_unwrap(env, _this, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return NULL;

  napi_value num;
  status = napi_create_number(env, obj->value_, &num);
  if (status != napi_ok) return NULL;

  return num;
}

napi_value MyObject::SetValue(napi_env env, napi_callback_info info) {
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

  MyObject* obj;
  status = napi_unwrap(env, _this, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return NULL;

  status = napi_get_value_double(env, args[0], &obj->value_);
  if (status != napi_ok) return NULL;
  return NULL;
}

napi_value MyObject::PlusOne(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 0;
  napi_value _this;
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    NULL,
    &_this,
    NULL);
  if (status != napi_ok) return NULL;

  MyObject* obj;
  status = napi_unwrap(env, _this, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return NULL;

  obj->value_ += 1;

  napi_value num;
  status = napi_create_number(env, obj->value_, &num);
  if (status != napi_ok) return NULL;

  return num;
}

napi_value MyObject::Multiply(napi_env env, napi_callback_info info) {
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

  double multiple = 1;
  if (argc >= 1) {
    status = napi_get_value_double(env, args[0], &multiple);
    if (status != napi_ok) return NULL;
  }

  MyObject* obj;
  status = napi_unwrap(env, _this, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return NULL;

  napi_value cons;
  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) return NULL;

  const int kArgCount = 1;
  napi_value argv[kArgCount];
  status = napi_create_number(env, obj->value_ * multiple, argv);
  if (status != napi_ok) return NULL;

  napi_value instance;
  status = napi_new_instance(env, cons, kArgCount, argv, &instance);
  if (status != napi_ok) return NULL;

  return instance;
}
