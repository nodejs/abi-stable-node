#include <string.h>
#include <string>
#include <node_api_helpers.h>

#define JS_ASSERT(env, assertion, message) \
  if (!(assertion)) { \
    napi_throw_error((env), (std::string("assertion (" #assertion ") failed: ") + message).c_str()); \
	return; \
  }

#define NAPI_CALL(env, theCall) \
  if (theCall != napi_ok) { \
    const char *errorMessage = napi_get_last_error_info()->error_message; \
	errorMessage = errorMessage ? errorMessage: "empty error message"; \
    napi_throw_error((env), errorMessage); \
	return; \
  }

static const char theText[] =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

static int deleterCallCount = 0;
static void deleteTheText(void *data) {
  delete (char *)data;
  deleterCallCount++;
}

static void noopDeleter(void *data) {
  deleterCallCount++;
}

NAPI_METHOD(newBuffer) {
  napi_value theBuffer;
  char *theCopy;
  NAPI_CALL(env, napi_create_buffer(env, sizeof(theText), &theCopy,
    &theBuffer));
  JS_ASSERT(env, theCopy, "Failed to copy static text for newBuffer");
  strcpy(theCopy, theText);
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

NAPI_METHOD(newExternalBuffer) {
  napi_value theBuffer;
  char *theCopy = strdup(theText);
  JS_ASSERT(env, theCopy,
    "Failed to copy static text for newExternalBuffer");
  NAPI_CALL(env, napi_create_external_buffer(env, sizeof(theText), theCopy,
    deleteTheText, &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

NAPI_METHOD(getDeleterCallCount) {
  napi_value callCount;
  NAPI_CALL(env, napi_create_number(env, deleterCallCount, &callCount));
  NAPI_CALL(env, napi_set_return_value(env, info, callCount));
}

NAPI_METHOD(copyBuffer) {
  napi_value theBuffer;
  NAPI_CALL(env, napi_create_buffer_copy(env, theText, sizeof(theText),
    &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

NAPI_METHOD(bufferHasInstance) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  bool hasInstance;
  NAPI_CALL(env, napi_is_buffer(env, theBuffer, &hasInstance));
  JS_ASSERT(env, hasInstance, "bufferHasInstance: instance is not a buffer");
  napi_value returnValue;
  NAPI_CALL(env, napi_create_boolean(env, hasInstance, &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

NAPI_METHOD(bufferInfo) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer, returnValue;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  char *bufferData;
  size_t bufferLength;
  NAPI_CALL(env, napi_get_buffer_info(env, theBuffer, &bufferData,
    &bufferLength));
  NAPI_CALL(env, napi_create_boolean(env,
    !strcmp(bufferData, theText) && bufferLength == sizeof(theText),
	&returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

NAPI_METHOD(staticBuffer) {
  napi_value theBuffer;
  NAPI_CALL(env, napi_create_external_buffer(env, sizeof(theText),
    (char *)theText, noopDeleter, &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_propertyname propName;
  napi_value theValue;

  NAPI_CALL(env, napi_property_name(env, "theText", &propName));
  NAPI_CALL(env, napi_create_string_utf8(env, theText, sizeof(theText), &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  napi_property_descriptor methods[] = {
    { "newBuffer", newBuffer },
	{ "newExternalBuffer", newExternalBuffer },
	{ "getDeleterCallCount", getDeleterCallCount },
	{ "copyBuffer", copyBuffer },
	{ "bufferHasInstance", bufferHasInstance },
	{ "bufferInfo", bufferInfo },
	{ "staticBuffer", staticBuffer }
  };

  NAPI_CALL(env, napi_define_properties(env, exports,
    sizeof(methods)/sizeof(methods[0]), methods));
}

NODE_MODULE_ABI(addon, Init)
