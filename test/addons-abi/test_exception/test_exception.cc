#include <node_jsvmapi.h>
#include <node_api_helpers.h>

NAPI_METHOD(returnException) {
	napi_trycatch tryCatch;
	napi_value returnValue;
	napi_value jsFunction;

	napi_get_cb_args(env, info, &jsFunction, 1);

	napi_trycatch_new(env, &tryCatch);

	// What is the return value when there was an exception?
	returnValue = napi_call_function(env, napi_get_global_scope(env),
		jsFunction, 0, 0);

	napi_set_return_value(env, info, napi_trycatch_exception(env, tryCatch));

	napi_trycatch_delete(env, tryCatch);
}

NAPI_METHOD(allowException) {
	napi_trycatch tryCatch;
	napi_value returnValue;
	napi_value jsFunction;

	napi_get_cb_args(env, info, &jsFunction, 1);

	napi_trycatch_new(env, &tryCatch);

	returnValue = napi_call_function(env, napi_get_global_scope(env),
		jsFunction, 0, 0);

	napi_trycatch_delete(env, tryCatch);
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
