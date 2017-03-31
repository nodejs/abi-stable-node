#include <node_api.h>

#if defined _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define NAPI_CHECK(env, condition, msg)                                   \
  if (!(condition)) {                                                     \
    napi_throw_error((env), (msg));                                       \
    return;                                                               \
  }

#define NAPI_CALL(env, theCall)                                           \
  if ((theCall) != napi_ok) {                                             \
    const napi_extended_error_info* error;                                \
    napi_get_last_error_info((env), &error);                              \
    const char *errorMessage = error->error_message;                      \
    errorMessage = errorMessage ? errorMessage : "empty error message";   \
    napi_throw_error((env), errorMessage);                                \
    return;                                                               \
  }

typedef struct {
  int32_t _input;
  int32_t _output;
  napi_ref _callback;
  napi_async_work _request;
} carrier;

carrier theCarrier;

struct AutoHandleScope {
  AutoHandleScope(napi_env env)
  : _env(env),
  _scope(NULL) {
    napi_open_handle_scope(_env, &_scope);
  }
  ~AutoHandleScope() {
    napi_close_handle_scope(_env, _scope);
  }
private:
  AutoHandleScope() { }

  napi_env _env;
  napi_handle_scope _scope;
};

void Execute(napi_env env, void* data) {
#if defined _WIN32
  Sleep(1000);
#else
  sleep(1);
#endif
  carrier* c = static_cast<carrier*>(data);
  
  if (c != &theCarrier) {
    napi_throw_type_error(env, "Wrong data parameter to Execute.");
    return;
  }
  
  c->_output = c->_input * 2;
}

void Complete(napi_env env, napi_status status, void* data) {
  AutoHandleScope scope(env);
  carrier* c = static_cast<carrier*>(data);

  if (c != &theCarrier) {
    napi_throw_type_error(env, "Wrong data parameter to Complete.");
    return;
  }
  
  if (status != napi_ok) {
    napi_throw_type_error(env, "Execute callback failed.");
    return;
  }

  napi_value argv[2];
  
  NAPI_CALL(env, napi_get_null(env, &argv[0]));
  NAPI_CALL(env, napi_create_number(env, c->_output, &argv[1]));
  napi_value callback;
  NAPI_CALL(env, napi_get_reference_value(env, c->_callback, &callback));
  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  napi_value result;
  NAPI_CALL(env, napi_call_function(env, global, callback, 2, argv, &result));
  
  NAPI_CALL(env, napi_delete_reference(env, c->_callback));
  NAPI_CALL(env, napi_delete_async_work(env, c->_request));
}

void Test(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_value _this;
  void* data;
  NAPI_CALL(env,
    napi_get_cb_info(env, info, &argc, argv, &_this, &data));
  NAPI_CHECK(env, argc >= 2, "Not enough arguments, expected 2.");

  napi_valuetype t;
  NAPI_CALL(env, napi_typeof(env, argv[0], &t));
  NAPI_CHECK(env, t == napi_number, "Wrong first argument, integer expected.");
  NAPI_CALL(env, napi_typeof(env, argv[1], &t));
  NAPI_CHECK(env, t == napi_function, "Wrong second argument,"
    " function expected.");

  theCarrier._output = 0;

  NAPI_CALL(env,
    napi_get_value_int32(env, argv[0], &theCarrier._input));
  NAPI_CALL(env,
    napi_create_reference(env, argv[1], 1, &theCarrier._callback));
  NAPI_CALL(env, napi_create_async_work(
    env, Execute, Complete, &theCarrier, &theCarrier._request));
  NAPI_CALL(env,
    napi_queue_async_work(env, theCarrier._request));
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_value test;
  NAPI_CALL(env, napi_create_function(env, "Test", Test, NULL, &test));
  NAPI_CALL(env, napi_set_named_property(env, module, "exports", test));
}

NAPI_MODULE(addon, Init)
