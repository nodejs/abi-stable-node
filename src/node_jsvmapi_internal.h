#ifndef SRC_NODE_JSVMAPI_INTERNAL_H_
#define SRC_NODE_JSVMAPI_INTERNAL_H_

#include "node_jsvmapi.h"

namespace v8impl {
  napi_env JsEnvFromV8Isolate(v8::Isolate* isolate);
  v8::Isolate* V8IsolateFromJsEnv(napi_env e);

  napi_value JsValueFromV8LocalValue(v8::Local<v8::Value> local);
  v8::Local<v8::Value> V8LocalValueFromJsValue(napi_value v);
}


struct napi_func_cb_info_impl {
  v8::Arguments* Arguments;
  v8::Persistent<v8::Value> ReturnValue;
};


#endif  // SRC_NODE_JSVMAPI_INTERNAL_H_
