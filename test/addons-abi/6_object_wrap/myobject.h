#ifndef TEST_ADDONS_ABI_6_OBJECT_WRAP_MYOBJECT_H_
#define TEST_ADDONS_ABI_6_OBJECT_WRAP_MYOBJECT_H_

#include <node_jsvmapi.h>

class MyObject {
 public:
  static void Init(napi_env env, napi_value exports);
  static void Destructor(void* nativeObject);

 private:
  explicit MyObject(double value_ = 0);
  ~MyObject();

  static void New(napi_env env, napi_callback_info info);
  static void GetValue(napi_env env, napi_callback_info info);
  static void SetValue(napi_env env, napi_callback_info info);
  static void PlusOne(napi_env env, napi_callback_info info);
  static void Multiply(napi_env env, napi_callback_info info);
  static void IsInstance(napi_env env, napi_callback_info info);
  static napi_ref constructor;
  double value_;
  napi_env env_;
  napi_ref wrapper_;
};

#endif  // TEST_ADDONS_ABI_6_OBJECT_WRAP_MYOBJECT_H_
