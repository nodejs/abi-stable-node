#include "myobject.h"

napi_persistent MyObject::constructor;

MyObject::MyObject(double value) : value_(value) {
}

MyObject::~MyObject() {
}

void MyObject::Destructor(void* nativeObject) {
  reinterpret_cast<MyObject*>(nativeObject)->~MyObject();
}

void MyObject::Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    { "value", nullptr, GetValue, SetValue },
    { "plusOne", PlusOne },
    { "multiply", Multiply },
  };
  napi_value cons = napi_create_constructor(env, "MyObject", New,
                                            nullptr, 3, properties);
  constructor = napi_create_persistent(env, cons);

  napi_set_property(env, exports, napi_property_name(env, "MyObject"),
                    cons);
}

void MyObject::New(napi_env env, napi_callback_info info) {
  if (napi_is_construct_call(env, info)) {
    // Invoked as constructor: `new MyObject(...)`
    napi_value args[1];
    napi_get_cb_args(env, info, args, 1);
    double value = 0;
    if (napi_get_undefined_(env) != args[0]) {
      value = napi_get_number_from_value(env, args[0]);
    }
    MyObject* obj = new MyObject(value);
    napi_value jsthis = napi_get_cb_this(env, info);
    napi_wrap(env, jsthis, reinterpret_cast<void*>(obj),
              MyObject::Destructor, nullptr);
    napi_set_return_value(env, info, jsthis);
  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call.
    napi_value args[1];
    napi_get_cb_args(env, info, args, 1);
    const int argc = 1;
    napi_value argv[argc] = { args[0] };
    napi_value cons = napi_get_persistent_value(env, constructor);
    napi_set_return_value(env, info, napi_new_instance(env, cons, argc, argv));
  }
}

void MyObject::GetValue(napi_env env, napi_callback_info info) {
  MyObject* obj = reinterpret_cast<MyObject*>(
                      napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, obj->value_));
}

void MyObject::SetValue(napi_env env, napi_callback_info info) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  MyObject* obj = reinterpret_cast<MyObject*>(
                      napi_unwrap(env, napi_get_cb_this(env, info)));
  obj->value_ = napi_get_number_from_value(env, value);
}

void MyObject::PlusOne(napi_env env, napi_callback_info info) {
  MyObject* obj = reinterpret_cast<MyObject*>(
                      napi_unwrap(env, napi_get_cb_this(env, info)));
  obj->value_ += 1;
  napi_set_return_value(env, info, napi_create_number(env, obj->value_));
}

void MyObject::Multiply(napi_env env, napi_callback_info info) {
  napi_value args[1];
  napi_get_cb_args(env, info, args, 1);

  double multiple = 1;
  if (napi_get_undefined_(env) != args[0]) {
    multiple = napi_get_number_from_value(env, args[0]);
  }

  MyObject* obj = reinterpret_cast<MyObject*>(
                      napi_unwrap(env, napi_get_cb_this(env, info)));

  napi_value cons = napi_get_persistent_value(env, constructor);
  const int argc = 1;
  napi_value argv[argc] = { napi_create_number(env, obj->value_ * multiple) };
  napi_set_return_value(env, info, napi_new_instance(env, cons, argc, argv));
}
