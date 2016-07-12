#include <node_jsvmapi.h>

void Test(napi_env napi_env, napi_func_cb_info info) {
  if (napi_get_cb_args_length(napi_env, info) < 1) {
    napi_throw_error(
          napi_env,
          napi_create_type_error(
              napi_env,
              napi_create_string(napi_env, "Wrong number of arguments")));
    return;
  }

  napi_value args[10];
  napi_get_cb_args(napi_env, info, args, 10);

  if (napi_get_type_of_value(napi_env, args[0]) != napi_function) {
    napi_throw_error(
        napi_env,
        napi_create_type_error(
             napi_env,
             napi_create_string(napi_env, "Wrong type of argments. Expects a function.")));
    return;
  }

  // v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(napi_env);
  // v8::EscapableHandleScope v8_scope(isolate);
  // napi_value scope = v8impl::JsValueFromV8LocalValue(v8_scope);
  napi_value scope = napi_get_global_scope(napi_env);

  napi_value function = args[0];
  int argc = napi_get_cb_args_length(napi_env, info) - 1;
  napi_value* argv = args + 1;
  napi_value output = napi_call_function(napi_env, scope, function, argc, argv);
  napi_set_return_value(napi_env, info, output);
}

void Init(napi_env napi_env, napi_value exports, napi_value module) {
  napi_set_property(napi_env, exports,
                    napi_property_name(napi_env, "Test"),
                    napi_create_function(napi_env, Test));
}

NODE_MODULE_ABI(addon, Init)
