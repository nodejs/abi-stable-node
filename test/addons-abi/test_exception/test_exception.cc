#include <node_jsvmapi.h>
#include <node_api_helpers.h>

NAPI_METHOD(returnException) {
	napi_value returnValue;
	napi_value jsFunction;

	napi_get_cb_args(env, info, &jsFunction, 1);

	// What is the return value when there was an exception?
	returnValue = napi_call_function(env, napi_get_global_scope(env),
		jsFunction, 0, 0);

	napi_set_return_value(env, info, napi_get_and_clear_last_exception(env));
}

NAPI_METHOD(allowException) {
	napi_value returnValue;
	napi_value jsFunction;

	napi_get_cb_args(env, info, &jsFunction, 1);

	returnValue = napi_call_function(env, napi_get_global_scope(env),
		jsFunction, 0, 0);
}

NAPI_MODULE_INIT(Init) {
  napi_set_property(env, exports,
                    napi_property_name(env, "returnException"),
                    napi_create_function(env, returnException));
  napi_set_property(env, exports,
                    napi_property_name(env, "allowException"),
                    napi_create_function(env, allowException));
}

NODE_MODULE_ABI(addon, Init)
