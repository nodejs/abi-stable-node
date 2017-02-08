#include <node_jsvmapi.h>
#include <string.h>

void Multiply(napi_env env, napi_callback_info info) {
  napi_status status;

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc != 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  status = napi_get_cb_args(env, info, args, 2);
  if (status != napi_ok) return;

  napi_valuetype valuetype0;
  status = napi_get_type_of_value(env, args[0], &valuetype0);
  if (status != napi_ok) return;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a typed array as first argument.");
    return;
  }

  napi_value input_array = args[0];
  bool istypedarray;
  status = napi_is_typedarray(env, input_array, &istypedarray);
  if (status != napi_ok) return;

  if (!istypedarray) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a typed array as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_get_type_of_value(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_number) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a number as second argument.");
    return;
  }

  double multiplier;
  status = napi_get_value_double(env, args[1], &multiplier);
  if (status != napi_ok) return;

  napi_typedarray_type type;
  napi_value input_buffer;
  size_t byte_offset;
  size_t length;
  status = napi_get_typedarray_info(
    env, input_array, &type, &length, nullptr, &input_buffer, &byte_offset);
  if (status != napi_ok) return;

  void* data;
  size_t byte_length;
  status = napi_get_arraybuffer_info(env, input_buffer, &data, &byte_length);
  if (status != napi_ok) return;

  napi_value output_buffer;
  void* output_ptr = nullptr;
  status = napi_create_arraybuffer(env, byte_length, &output_ptr, &output_buffer);
  if (status != napi_ok) return;

  napi_value output_array;
  status = napi_create_typedarray(env, type, length, output_buffer, byte_offset, &output_array);
  if (status != napi_ok) return;

  if (type == napi_uint8) {
    uint8_t* input_bytes = reinterpret_cast<uint8_t*>(data) + byte_offset;
    uint8_t* output_bytes = reinterpret_cast<uint8_t*>(output_ptr);
    for (size_t i = 0; i < length; i++) {
      output_bytes[i] = static_cast<uint8_t>(input_bytes[i] * multiplier);
    }
  }
  else if (type == napi_float64) {
    double* input_doubles = reinterpret_cast<double*>(
      reinterpret_cast<uint8_t*>(data) + byte_offset);
    double* output_doubles = reinterpret_cast<double*>(output_ptr);
    for (size_t i = 0; i < length; i++) {
      output_doubles[i] = input_doubles[i] * multiplier;
    }
  }
  else {
    napi_throw_error(env, "Typed array was of a type not expected by test.");
    return;
  }

  status = napi_set_return_value(env, info, output_array);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
    { "Multiply", Multiply },
  };

  for (int i = 0; i < sizeof(descriptors) / sizeof(*descriptors); i++) {
    status = napi_define_property(env, exports, &descriptors[i]);
    if (status != napi_ok) return;
  }
}

NODE_MODULE_ABI(addon, Init)
