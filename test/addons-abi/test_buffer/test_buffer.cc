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
  char *theCopy = strdup(theText);
  JS_ASSERT(env, theCopy,
    "Failed to copy static text for newBuffer");
  NAPI_CALL(env, napi_create_buffer(env, theCopy, sizeof(theText),
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
  napi_value returnValue;
  NAPI_CALL(env, napi_create_boolean(env, hasInstance, &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

NAPI_METHOD(bufferData) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  char *bufferData;
  NAPI_CALL(env, napi_get_buffer_data(env, theBuffer, &bufferData));
  bool stringsEqual = !strcmp(bufferData, theText);
  napi_value returnValue;
  NAPI_CALL(env, napi_create_boolean(env, stringsEqual, &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

NAPI_METHOD(staticBuffer) {
  napi_value theBuffer;
  NAPI_CALL(env, napi_create_buffer(env, (char *)theText,
    sizeof(theText), noopDeleter, &theBuffer));
  NAPI_CALL(env, napi_set_return_value(env, info, theBuffer));
}

NAPI_METHOD(bufferLength) {
  int argc;
  NAPI_CALL(env, napi_get_cb_args_length(env, info, &argc));
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer;
  NAPI_CALL(env, napi_get_cb_args(env, info, &theBuffer, 1));
  size_t result;
  NAPI_CALL(env, napi_get_buffer_length(env, theBuffer, &result));
  napi_value returnValue;
  NAPI_CALL(env, napi_create_number(env, result, &returnValue));
  NAPI_CALL(env, napi_set_return_value(env, info, returnValue));
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_propertyname propName;
  napi_value theValue;

  NAPI_CALL(env, napi_property_name(env, "theText", &propName));
  NAPI_CALL(env, napi_create_string_utf8(env, theText, sizeof(theText), &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "newBuffer", &propName));
  NAPI_CALL(env, napi_create_function(env, newBuffer, nullptr, &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "getDeleterCallCount", &propName));
  NAPI_CALL(env, napi_create_function(env, getDeleterCallCount, nullptr,
    &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "copyBuffer", &propName));
  NAPI_CALL(env, napi_create_function(env, copyBuffer, nullptr, &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "bufferHasInstance", &propName));
  NAPI_CALL(env, napi_create_function(env, bufferHasInstance, nullptr,
    &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "bufferData", &propName));
  NAPI_CALL(env, napi_create_function(env, bufferData, nullptr, &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "staticBuffer", &propName));
  NAPI_CALL(env, napi_create_function(env, staticBuffer, nullptr, &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));

  NAPI_CALL(env, napi_property_name(env, "bufferLength", &propName));
  NAPI_CALL(env, napi_create_function(env, bufferLength, nullptr, &theValue));
  NAPI_CALL(env, napi_set_property(env, exports, propName, theValue));
}

NODE_MODULE_ABI(addon, Init)
