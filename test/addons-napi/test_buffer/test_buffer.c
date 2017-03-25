#include <stdlib.h>
#include <string.h>
#include <node_api.h>

#define JS_ASSERT(env, assertion, message)              \
  if (!(assertion)) {                                   \
    napi_throw_error(                                   \
        (env),                                          \
        "assertion (" #assertion ") failed: " message); \
    return NULL;                                        \
  }

#define GET_AND_THROW_LAST_ERROR                        \
  const char *errorMessage = napi_get_last_error_info()->error_message; \
  errorMessage = errorMessage ? errorMessage : "empty error message";   \
  napi_throw_error((env), errorMessage);                                \

#define NAPI_CALL(env, theCall)                                         \
  if ((theCall) != napi_ok) {                                           \
    GET_AND_THROW_LAST_ERROR(env);                                      \
    return NULL;                                                        \
  }

#define NAPI_CALL_NO_RETURN(env, theCall)                               \
  if ((theCall) != napi_ok) {                                           \
    GET_AND_THROW_LAST_ERROR(env);                                      \
  }

static const char theText[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

static int deleterCallCount = 0;
static void deleteTheText(void *data, void* finalize_hint) {
  (void)finalize_hint;
  free(data);
  deleterCallCount++;
}

static void noopDeleter(void *data, void* finalize_hint) {
  (void)finalize_hint;
  deleterCallCount++;
}

napi_value newBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  char *theCopy;
  const unsigned int kBufferSize = sizeof(theText);

  NAPI_CALL(env,
            napi_create_buffer(
                env,
                sizeof(theText),
                (void **)(&theCopy),
                &theBuffer));
  JS_ASSERT(env, theCopy, "Failed to copy static text for newBuffer");
  memcpy(theCopy, theText, kBufferSize);

  return theBuffer;
}

napi_value newExternalBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  char *theCopy = strdup(theText);
  JS_ASSERT(env, theCopy, "Failed to copy static text for newExternalBuffer");
  NAPI_CALL(env,
            napi_create_external_buffer(
                env,
                sizeof(theText),
                theCopy,
                deleteTheText,
                NULL,  // finalize_hint
                &theBuffer));

  return theBuffer;
}

napi_value getDeleterCallCount(napi_env env, napi_callback_info info) {
  napi_value callCount;
  NAPI_CALL(env, napi_create_number(env, deleterCallCount, &callCount));
  return callCount;
}

napi_value copyBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  NAPI_CALL(env, napi_create_buffer_copy(
    env, sizeof(theText), theText, NULL, &theBuffer));
  return theBuffer;
}

napi_value bufferHasInstance(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer = args[0];
  bool hasInstance;
  napi_valuetype theType;
  NAPI_CALL(env, napi_typeof(env, theBuffer, &theType));
  JS_ASSERT(env,
            theType == napi_object,
            "bufferHasInstance: instance is not an object");
  NAPI_CALL(env, napi_is_buffer(env, theBuffer, &hasInstance));
  JS_ASSERT(env, hasInstance, "bufferHasInstance: instance is not a buffer");
  napi_value returnValue;
  NAPI_CALL(env, napi_get_boolean(env, hasInstance, &returnValue));
  return returnValue;
}

napi_value bufferInfo(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(
    env,
    info,
    &argc,
    args,
    NULL,
    NULL);
  if (status != napi_ok) return NULL;
  JS_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value theBuffer = args[0];
  napi_value returnValue;
  char *bufferData;
  size_t bufferLength;
  NAPI_CALL(env,
            napi_get_buffer_info(
                env,
                theBuffer,
                (void **)(&bufferData),
                &bufferLength));
  NAPI_CALL(env, napi_get_boolean(env,
    !strcmp(bufferData, theText) && bufferLength == sizeof(theText),
    &returnValue));
  return returnValue;
}

napi_value staticBuffer(napi_env env, napi_callback_info info) {
  napi_value theBuffer;
  NAPI_CALL(
      env,
      napi_create_external_buffer(env,
                                  sizeof(theText),
                                  (void *)theText,
                                  noopDeleter,
                                  NULL,  // finalize_hint
                                  &theBuffer));
  return theBuffer;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_value theValue;

  NAPI_CALL_NO_RETURN(env, napi_create_string_utf8(env,
            theText, sizeof(theText), &theValue));
  NAPI_CALL_NO_RETURN(env, 
            napi_set_named_property(env, exports, "theText", theValue));

  napi_property_descriptor methods[] = {
      DECLARE_NAPI_METHOD("newBuffer", newBuffer),
      DECLARE_NAPI_METHOD("newExternalBuffer", newExternalBuffer),
      DECLARE_NAPI_METHOD("getDeleterCallCount", getDeleterCallCount),
      DECLARE_NAPI_METHOD("copyBuffer", copyBuffer),
      DECLARE_NAPI_METHOD("bufferHasInstance", bufferHasInstance),
      DECLARE_NAPI_METHOD("bufferInfo", bufferInfo),
      DECLARE_NAPI_METHOD("staticBuffer", staticBuffer),
  };
  NAPI_CALL_NO_RETURN(env,
            napi_define_properties(
                env, exports, sizeof(methods) / sizeof(methods[0]), methods));
}

NAPI_MODULE(addon, Init)
