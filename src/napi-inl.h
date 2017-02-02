#ifndef SRC_NAPI_INL_H_
#define SRC_NAPI_INL_H_

////////////////////////////////////////////////////////////////////////////////
// NAPI C++ Wrapper Classes
//
// Inline header-only implementations for "NAPI" ABI-stable C APIs for Node.js.
////////////////////////////////////////////////////////////////////////////////

#include <cassert>

namespace Napi {

// Adapt the NODE_MODULE registration function to NAPI:
//  - Wrap the arguments in NAPI wrappers.
//  - Catch any NAPI errors that might be thrown.
#define NAPI_MODULE(modname, regfunc)                                                 \
  void __napi_ ## regfunc(napi_env env, napi_value exports, napi_value module) {      \
    try {                                                                             \
      regfunc(Napi::Env(env), Napi::Object(env, exports), Napi::Object(env, module)); \
    }                                                                                 \
    catch (const Napi::Error&) {                                                      \
      assert(false); /* Uncaught error in native module registration. */              \
    }                                                                                 \
  }                                                                                   \
  NODE_MODULE_ABI(modname, __napi_ ## regfunc);


////////////////////////////////////////////////////////////////////////////////
// Env class
////////////////////////////////////////////////////////////////////////////////

inline Env::Env(napi_env env) : _env(env) {
}

inline Env::operator napi_env() const {
  return _env;
}
inline Env Env::Current() {
  napi_env env;
  napi_status status = napi_get_current_env(&env);
  if (status != napi_ok) env = nullptr; // Can't throw without an environment!
  return Env(env);
}

inline Object Env::Global() const {
  napi_value value;
  napi_status status = napi_get_global(*this, &value);
  if (status != napi_ok) throw const_cast<Env*>(this)->NewError();
  return Object(*this, value);
}

inline Value Env::Undefined() const {
  napi_value value;
  napi_status status = napi_get_undefined(*this, &value);
  if (status != napi_ok) throw const_cast<Env*>(this)->NewError();
  return Value(*this, value);
}

inline Value Env::Null() const {
  napi_value value;
  napi_status status = napi_get_null(*this, &value);
  if (status != napi_ok) throw const_cast<Env*>(this)->NewError();
  return Value(*this, value);
}

inline Error Env::NewError() {
  napi_value error;
  if (IsExceptionPending()) {
    napi_get_and_clear_last_exception(_env, &error);
  }
  else {
    // No JS exception is pending, so check for NAPI error info.
    const napi_extended_error_info* info = napi_get_last_error_info();

    napi_value message;
    napi_status status = napi_create_string_utf8(
      _env,
      info->error_message != nullptr ? info->error_message : "Error in native callback",
      -1,
      &message);
    assert(status == napi_ok);

    if (status == napi_ok) {
      switch (info->error_code) {
        case napi_object_expected:
        case napi_string_expected:
        case napi_boolean_expected:
        case napi_number_expected:
          status = napi_create_type_error(_env, message, &error);
          break;
        default:
          status = napi_create_error(_env, message, &error);
          break;
      }
      assert(status == napi_ok);
    }
  }

  return Error(_env, error);
}

inline Error Env::NewError(const char* message) {
  napi_value str;
  napi_status status = napi_create_string_utf8(*this, message, -1, &str);
  if (status != napi_ok) throw NewError();

  napi_value error;
  status = napi_create_error(*this, str, &error);
  if (status != napi_ok) throw NewError();

  return Error(*this, error);
}

inline Error Env::NewTypeError(const char* message) {
  napi_value str;
  napi_status status = napi_create_string_utf8(*this, message, -1, &str);
  if (status != napi_ok) throw NewError();

  napi_value error;
  status = napi_create_error(*this, str, &error);
  if (status != napi_ok) throw NewError();

  return Error(*this, error);
}

inline Buffer Env::NewBuffer(char* data, size_t size) {
  napi_value value;
  napi_status status = napi_buffer_new(*this, data, size, &value);
  if (status != napi_ok) throw NewError();
  return Buffer(*this, value);
}

inline Buffer Env::CopyBuffer(const char* data, size_t size) {
  napi_value value;
  napi_status status = napi_buffer_copy(*this, data, size, &value);
  if (status != napi_ok) throw NewError();
  return Buffer(*this, value);
}

inline Number Env::NewNumber(double val) {
  napi_value value;
  napi_status status = napi_create_number(_env, val, &value);
  if (status != napi_ok) throw NewError();
  return Number(_env, value);
}

inline Boolean Env::NewBoolean(bool val) {
  napi_value value;
  napi_status status = napi_create_boolean(_env, val, &value);
  if (status != napi_ok) throw NewError();
  return Boolean(_env, value);
}

inline String Env::NewStringUtf8(const char* val, int length) {
  napi_value value;
  napi_status status = napi_create_string_utf8(_env, val, length, &value);
  if (status != napi_ok) throw NewError();
  return String(_env, value);
}

inline String Env::NewStringUtf16(const char16_t* val, int length) {
  napi_value value;
  napi_status status = napi_create_string_utf16(_env, val, length, &value);
  if (status != napi_ok) throw NewError();
  return String(_env, value);
}

inline Object Env::NewObject() {
  napi_value value;
  napi_status status = napi_create_object(_env, &value);
  if (status != napi_ok) throw NewError();
  return Object(_env, value);
}

inline Array Env::NewArray() {
  napi_value value;
  napi_status status = napi_create_array(_env, &value);
  if (status != napi_ok) throw NewError();
  return Array(_env, value);
}

inline Array Env::NewArray(int length) {
  napi_value value;
  napi_status status = napi_create_array_with_length(_env, length, &value);
  if (status != napi_ok) throw NewError();
  return Array(_env, value);
}

inline Function Env::NewFunction(VoidFunctionCallback cb, const char* utf8name, void* data) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the function is destroyed
  callbackData->voidFunctionCallback = cb;
  callbackData->data = data;

  // TODO: set the function name
  napi_value value;
  napi_status status = napi_create_function(
    _env, VoidFunctionCallbackWrapper, callbackData, &value);
  if (status != napi_ok) throw NewError();
  return Function(_env, value);
}

inline Function Env::NewFunction(FunctionCallback cb, const char* utf8name, void* data) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the function is destroyed
  callbackData->functionCallback = cb;
  callbackData->data = data;

  // TODO: set the function name
  napi_value value;
  napi_status status = napi_create_function(
    _env, FunctionCallbackWrapper, callbackData, &value);
  if (status != napi_ok) throw NewError();
  return Function(_env, value);
}

inline void Env::VoidFunctionCallbackWrapper(napi_env env, napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    callbackData->functionCallback(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }
}

inline void Env::FunctionCallbackWrapper(napi_env env, napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    result = callbackData->functionCallback(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  napi_status status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

inline bool Env::IsExceptionPending() const {
  bool result;
  napi_status status = napi_is_exception_pending(_env, &result);
  if (status != napi_ok) result = false; // Checking for a pending exception shouldn't throw.
  return result;
}

inline void Env::Throw(Value error) {
  napi_throw(_env, error);
  throw NewError();
}

inline void Env::ThrowError(const char* message) {
  napi_throw_error(_env, message);
  throw NewError();
}

inline void Env::ThrowTypeError(const char* message) {
  napi_throw_type_error(_env, message);
  throw NewError();
}

////////////////////////////////////////////////////////////////////////////////
// PropertyName class
////////////////////////////////////////////////////////////////////////////////

inline PropertyName::PropertyName(napi_env env, napi_propertyname name) : _env(env), _name(name) {
}

inline PropertyName::PropertyName(napi_env env, const char* utf8name) : _env(env) {
  napi_status status = napi_property_name(env, utf8name, &_name);
  if (status != napi_ok) throw Env().NewError();
}

inline PropertyName::operator napi_propertyname() const {
  return _name;
}

inline Env PropertyName::Env() const {
  return Napi::Env(_env);
}

////////////////////////////////////////////////////////////////////////////////
// Value class
////////////////////////////////////////////////////////////////////////////////

inline Value::Value() : _env(nullptr), _value(nullptr) {
}

inline Value::Value(napi_env env, napi_value value) : _env(env), _value(value) {
}

inline Value::operator napi_value() const {
  return _value;
}

inline bool Value::operator ==(const Value& other) const {
  return StrictEquals(other);
}

inline bool Value::operator !=(const Value& other) const {
  return !StrictEquals(other);
}

inline bool Value::StrictEquals(const Value& other) const {
  bool result;
  napi_status status = napi_strict_equals(_env, *this, other, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline Persistent Value::MakePersistent() const {
  napi_persistent persistent;
  napi_status status = napi_create_persistent(_env, _value, &persistent);
  if (status != napi_ok) throw Env().NewError();
  return Persistent(_env, persistent);
}

inline Env Value::Env() const {
  return Napi::Env(_env);
}

inline napi_valuetype Value::Type() const {
  if (_value == nullptr) {
    return napi_undefined;
  }

  napi_valuetype type;
  napi_status status = napi_get_type_of_value(_env, _value, &type);
  if (status != napi_ok) throw Env().NewError();
  return type;
}

inline bool Value::IsUndefined() const {
  return Type() == napi_undefined;
}

inline bool Value::IsNull() const {
  return Type() == napi_null;
}

inline bool Value::IsBoolean() const {
  return Type() == napi_boolean;
}

inline bool Value::IsNumber() const {
  return Type() == napi_number;
}

inline bool Value::IsString() const {
  return Type() == napi_string;
}

inline bool Value::IsSymbol() const {
  return Type() == napi_symbol;
}

inline bool Value::IsArray() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_is_array(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline bool Value::IsObject() const {
  return Type() == napi_object;
}

inline bool Value::IsFunction() const {
  return Type() == napi_function;
}

inline bool Value::IsBuffer() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_buffer_has_instance(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline Boolean Value::AsBoolean() const {
  return Boolean(_env, _value);
}

inline Number Value::AsNumber() const {
  return Number(_env, _value);
}

inline String Value::AsString() const {
  return String(_env, _value);
}

inline Object Value::AsObject() const {
  return Object(_env, _value);
}

inline Array Value::AsArray() const {
  return Array(_env, _value);
}

inline Function Value::AsFunction() const {
  return Function(_env, _value);
}

inline Buffer Value::AsBuffer() const {
  return Buffer(_env, _value);
}

inline Boolean Value::ToBoolean() const {
  napi_value result;
  napi_status status = napi_coerce_to_bool(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return Boolean(_env, result);
}

inline Number Value::ToNumber() const {
  napi_value result;
  napi_status status = napi_coerce_to_number(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return Number(_env, result);
}

inline String Value::ToString() const {
  napi_value result;
  napi_status status = napi_coerce_to_string(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return String(_env, result);
}

inline Object Value::ToObject() const {
  napi_value result;
  napi_status status = napi_coerce_to_object(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return Object(_env, result);
}

////////////////////////////////////////////////////////////////////////////////
// Boolean class
////////////////////////////////////////////////////////////////////////////////

inline Boolean::Boolean() : Napi::Value() {
}

inline Boolean::Boolean(napi_env env, napi_value value) : Napi::Value(env, value) {
}

inline Boolean::operator bool() const {
  return Value();
}

inline bool Boolean::Value() const {
  bool result;
  napi_status status = napi_get_value_bool(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Number class
////////////////////////////////////////////////////////////////////////////////

inline Number::Number() : Value() {
}

inline Number::Number(napi_env env, napi_value value) : Value(env, value) {
}

inline Number::operator int32_t() const {
  return Int32Value();
}

inline Number::operator uint32_t() const {
  return Uint32Value();
}

inline Number::operator int64_t() const {
  return Int64Value();
}

inline Number::operator float() const {
  return FloatValue();
}

inline Number::operator double() const {
  return DoubleValue();
}

inline int32_t Number::Int32Value() const {
  int32_t result;
  napi_status status = napi_get_value_int32(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline uint32_t Number::Uint32Value() const {
  uint32_t result;
  napi_status status = napi_get_value_uint32(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline int64_t Number::Int64Value() const {
  int64_t result;
  napi_status status = napi_get_value_int64(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline float Number::FloatValue() const {
  return static_cast<float>(DoubleValue());
}

inline double Number::DoubleValue() const {
  double result;
  napi_status status = napi_get_value_double(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// String class
////////////////////////////////////////////////////////////////////////////////

inline String::String() : Value() {
}

inline String::String(napi_env env, napi_value value) : Value(env, value) {
}

inline String::operator std::string() const {
  return Utf8Value();
}

inline String::operator std::u16string() const {
  return Utf16Value();
}

inline std::string String::Utf8Value() const {
  int length;
  napi_status status = napi_get_value_string_utf8_length(_env, _value, &length);
  if (status != napi_ok) throw Env().NewError();

  std::string value;
  value.resize(length);
  status = napi_get_value_string_utf8(_env, _value, &value[0], value.capacity(), nullptr);
  if (status != napi_ok) throw Env().NewError();
  return value;
}

inline std::u16string String::Utf16Value() const {
  int length;
  napi_status status = napi_get_value_string_utf16_length(_env, _value, &length);
  if (status != napi_ok) throw Env().NewError();

  std::u16string value;
  value.resize(length);
  status = napi_get_value_string_utf16(_env, _value, &value[0], value.capacity(), nullptr);
  if (status != napi_ok) throw Env().NewError();
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Object class
////////////////////////////////////////////////////////////////////////////////

inline Object::Object() : Value() {
}

inline Object::Object(napi_env env, napi_value value) : Value(env, value) {
}

inline Value Object::operator [](const char* name) const {
  return Get(name);
}

inline Value Object::Get(const PropertyName& name) const {
  napi_value result;
  napi_status status = napi_get_property(_env, _value, name, &result);
  if (status != napi_ok) throw Env().NewError();
  return Value(_env, result);
}

inline Value Object::Get(const char* utf8name) const {
  return Get(PropertyName(_env, utf8name));
}

inline void Object::Set(const PropertyName& name, const Value& value) {
  napi_status status = napi_set_property(_env, _value, name, value);
  if (status != napi_ok) throw Env().NewError();
}

inline void Object::Set(const char* utf8name, const Value& value) {
  Set(PropertyName(_env, utf8name), value);
}

inline void Object::Set(const char* utf8name, const char* utf8value) {
  Set(PropertyName(_env, utf8name), Env().NewStringUtf8(utf8value));
}

inline void Object::Set(const char* utf8name, bool boolValue) {
  Set(PropertyName(_env, utf8name), Env().NewBoolean(boolValue));
}

inline void Object::Set(const char* utf8name, double numberValue) {
  Set(PropertyName(_env, utf8name), Env().NewNumber(numberValue));
}

inline void Object::DefineProperty(const PropertyDescriptor& property) {
  napi_status status = napi_define_properties(_env, _value, 1,
    reinterpret_cast<const napi_property_descriptor*>(&property));
  if (status != napi_ok) throw Env().NewError();
}

inline void Object::DefineProperties(const std::vector<PropertyDescriptor>& properties) {
  napi_status status = napi_define_properties(_env, _value, properties.size(),
    reinterpret_cast<const napi_property_descriptor*>(properties.data()));
  if (status != napi_ok) throw Env().NewError();
}

inline bool Object::InstanceOf(const Value& constructor) const {
  bool result;
  napi_status status = napi_instanceof(_env, _value, constructor, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

template<typename T>
inline T* Object::Unwrap() const {
  T* unwrapped;
  napi_status status = napi_unwrap(_env, _value, reinterpret_cast<void**>(&unwrapped));
  if (status != napi_ok) throw Env().NewError();
  return unwrapped;
}

////////////////////////////////////////////////////////////////////////////////
// Array class
////////////////////////////////////////////////////////////////////////////////

inline Array::Array() : Value() {
}

inline Array::Array(napi_env env, napi_value value) : Value(env, value) {
}

inline Value Array::operator [](uint32_t index) const {
  return Get(index);
}

inline uint32_t Array::Length() const {
  uint32_t result;
  napi_status status = napi_get_array_length(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline bool Array::Has(uint32_t index) const {
  bool result;
  napi_status status = napi_has_element(_env, _value, index, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline Value Array::Get(uint32_t index) const {
  napi_value value;
  napi_status status = napi_get_element(_env, _value, index, &value);
  if (status != napi_ok) throw Env().NewError();
  return Value(_env, value);
}

inline void Array::Set(uint32_t index, const Value& value) {
  napi_status status = napi_set_element(_env, _value, index, value);
  if (status != napi_ok) throw Env().NewError();
}

inline void Array::Set(uint32_t index, const char* utf8value) {
  Set(index, Env().NewStringUtf8(utf8value));
}

inline void Array::Set(uint32_t index, bool boolValue) {
  Set(index, Env().NewBoolean(boolValue));
}

inline void Array::Set(uint32_t index, double numberValue) {
  Set(index, Env().NewNumber(numberValue));
}

////////////////////////////////////////////////////////////////////////////////
// Function class
////////////////////////////////////////////////////////////////////////////////

inline Function::Function() : Value() {
}

inline Function::Function(napi_env env, napi_value value) : Value(env, value) {
}

inline Value Function::operator ()(const std::vector<Value>& args) const {
  return Call(args);
}

inline Value Function::operator ()(Object& recv, const std::vector<Value>& args) const {
  return Call(recv, args);
}

inline Value Function::Call(const std::vector<Value>& args) const {
  Object global = Env().Global();
  return Call(global, args);
}

inline Value Function::Call(Object& recv, const std::vector<Value>& args) const {
  // Convert args from Value to napi_value.
  std::vector<napi_value> argv;
  argv.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    argv.push_back(args[i]);
  }

  napi_value result;
  napi_status status = napi_call_function(_env, recv, _value, argv.size(), argv.data(), &result);
  if (status != napi_ok) throw Env().NewError();
  return Value(_env, result);
}

inline Value Function::MakeCallback(const std::vector<Value>& args) const {
  Value global = Env().Global();
  return MakeCallback(global, args);
}

inline Value Function::MakeCallback(
  Value& recv, const std::vector<Value>& args) const {
  // Convert args from Value to napi_value.
  std::vector<napi_value> argv;
  argv.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    argv.push_back(args[i]);
  }

  napi_value result;
  napi_status status = napi_make_callback(_env, recv, _value, argv.size(), argv.data(), &result);
  if (status != napi_ok) throw Env().NewError();
  return Value(_env, result);
}

inline Object Function::New(const std::vector<Value>& args) {
  // Convert args from Value to napi_value.
  std::vector<napi_value> argv;
  argv.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    argv.push_back(args[i]);
  }

  napi_value result;
  napi_status status = napi_new_instance(_env, _value, argv.size(), argv.data(), &result);
  if (status != napi_ok) throw Env().NewError();
  return Object(_env, result);
}

////////////////////////////////////////////////////////////////////////////////
// Buffer class
////////////////////////////////////////////////////////////////////////////////

inline Buffer::Buffer() : Value() {
}

inline Buffer::Buffer(napi_env env, napi_value value) : Value(env, value) {
}

inline size_t Buffer::Length() const {
  size_t result;
  napi_status status = napi_buffer_length(_env, _value, &result);
  if (status != napi_ok) throw Env().NewError();
  return result;
}

inline char* Buffer::Data() const {
  char* data;
  napi_status status = napi_buffer_data(_env, _value, &data);
  if (status != napi_ok) throw Env().NewError();
  return data;
}

////////////////////////////////////////////////////////////////////////////////
// Error class
////////////////////////////////////////////////////////////////////////////////

inline Error::Error() : Value(), _message(nullptr) {
}

inline Error::Error(napi_env env, napi_value value) : Value(env, value) {
}

inline std::string Error::Message() const {
  if (_message.size() == 0 && _env != nullptr) {
    try {
      const_cast<Error*>(this)->_message = this->AsObject()["message"].AsString();
    }
    catch (const Error&) {
    }
  }
  return _message;
}

inline void Error::ThrowAsJavaScriptException() const {
  if (_value != nullptr) {
    napi_throw(_env, _value);
  }
}

inline const char* Error::what() const {
  return Message().c_str();
}

////////////////////////////////////////////////////////////////////////////////
// Persistent class
////////////////////////////////////////////////////////////////////////////////

inline Persistent::Persistent() : _env(nullptr), _persistent(nullptr) {
}

inline Persistent::Persistent(napi_env env, napi_persistent persistent)
  : _env(env), _persistent(persistent) {
}

inline Persistent::~Persistent() {
  if (_persistent != nullptr) {
    napi_release_persistent(_env, _persistent);
    _persistent = nullptr;
  }
}

inline Persistent::Persistent(Persistent&& other) {
  _env = other._env;
  _persistent = other._persistent;
  other._env = nullptr;
  other._persistent = nullptr;
}

inline Persistent& Persistent::operator =(Persistent&& other) {
  _env = other._env;
  _persistent = other._persistent;
  other._env = nullptr;
  other._persistent = nullptr;
  return *this;
}

inline Persistent::operator napi_persistent() const {
  return _persistent;
}

inline Env Persistent::Env() const {
  return Napi::Env(_env);
}

inline Value Persistent::Value() const {
  if (_persistent == nullptr) {
    return Napi::Value(_env, nullptr);
  }

  napi_value value;
  napi_status status = napi_get_persistent_value(_env, _persistent, &value);
  if (status != napi_ok) throw Env().NewError();
  return Napi::Value(_env, value);
}

////////////////////////////////////////////////////////////////////////////////
// CallbackInfo class
////////////////////////////////////////////////////////////////////////////////

inline CallbackInfo::CallbackInfo(napi_env env, napi_callback_info info)
    : _env(env), _this(nullptr), _args(), _data(nullptr) {
  napi_status status = napi_get_cb_this(env, info, &_this);
  if (status != napi_ok) throw Env().NewError();

  int argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) throw Env().NewError();

  if (argc > 0) {
    _args.resize(argc);
    status = napi_get_cb_args(env, info, _args.data(), argc);
    if (status != napi_ok) throw Env().NewError();
  }

  status = napi_get_cb_data(env, info, reinterpret_cast<void**>(&_data));
  if (status != napi_ok) throw Env().NewError();
}

inline Env CallbackInfo::Env() const {
  return Napi::Env(_env);
}

inline int CallbackInfo::Length() const {
  return _args.size();
}

inline const Value CallbackInfo::operator[](int index) const {
  return static_cast<size_t>(index) < _args.size() ?
    Value(_env, _args[index]) : Env().Undefined();
}

inline Object CallbackInfo::This() const {
  if (_this == nullptr) {
    return Env().Global();
  }
  return Object(_env, _this);
}

inline void* CallbackInfo::Data() const {
  return _data;
}

////////////////////////////////////////////////////////////////////////////////
// ObjectWrap<T> class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline ObjectWrap<T>::ObjectWrap() {
}

template <typename T>
inline Object ObjectWrap<T>::Wrapper() const {
  return _wrapper.Value().AsObject();
}

template <typename T>
inline Function ObjectWrap<T>::DefineClass(
    Env env,
    const char* utf8name,
    ConstructorCallback constructor,
    const std::vector<ClassPropertyDescriptor<T>>& properties,
    void* data = nullptr) {
  std::vector<napi_property_descriptor> staticProperties;
  staticProperties.reserve(properties.size());
  std::vector<napi_property_descriptor> instanceProperties;
  instanceProperties.reserve(properties.size());

  for (auto i = properties.begin(); i != properties.end(); i++) {
    const napi_property_descriptor& p = *i;
    if ((p.attributes & napi_instance_property) == 0) {
      staticProperties.push_back(p);
    }
    else {
      instanceProperties.push_back(p);
      napi_property_descriptor& q = instanceProperties.back();
      q.attributes = static_cast<napi_property_attributes>(
        q.attributes & ~napi_instance_property);
    }
  }

  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->constructorCallback = constructor;
  callbackData->data = data;

  napi_value value;
  napi_status status = napi_define_class(env, utf8name,
    T::ConstructorCallbackWrapper, callbackData, instanceProperties.size(),
    instanceProperties.data(), &value);
  if (status != napi_ok) throw env.NewError();

  status = napi_define_properties(env, value, staticProperties.size(),
    reinterpret_cast<const napi_property_descriptor*>(staticProperties.data()));
  if (status != napi_ok) throw env.NewError();

  return Function(env, value);
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticMethod(
    const char* utf8name,
    StaticVoidMethodCallback method,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->staticVoidMethodCallback = method;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.method = T::StaticVoidMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticMethod(
    const char* utf8name,
    StaticMethodCallback method,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->staticMethodCallback = method;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.method = T::StaticMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticAccessor(
    const char* utf8name,
    StaticGetterCallback getter,
    StaticSetterCallback setter,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->staticGetterCallback = getter;
  callbackData->staticSetterCallback = setter;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.getter = getter != nullptr ? T::StaticGetterCallbackWrapper : nullptr;
  desc.setter = setter != nullptr ? T::StaticSetterCallbackWrapper : nullptr;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceMethod(
    const char* utf8name,
    InstanceVoidMethodCallback method,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->instanceVoidMethodCallback = method;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.method = T::InstanceVoidMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(
    attributes | napi_instance_property);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceMethod(
    const char* utf8name,
    InstanceMethodCallback method,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->instanceMethodCallback = method;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.method = T::InstanceMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(
    attributes | napi_instance_property);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceAccessor(
    const char* utf8name,
    InstanceGetterCallback getter,
    InstanceSetterCallback setter,
    napi_property_attributes attributes = napi_default,
    void* data = nullptr) {
  CallbackData* callbackData = new CallbackData({}); // TODO: Delete when the class is destroyed
  callbackData->instanceGetterCallback = getter;
  callbackData->instanceSetterCallback = setter;
  callbackData->data = data;

  napi_property_descriptor desc = { utf8name };
  desc.getter = T::InstanceGetterCallbackWrapper;
  desc.setter = T::InstanceSetterCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(
    attributes | napi_instance_property);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticValue(const char* utf8name,
    Value value, napi_property_attributes attributes = napi_default) {
  napi_property_descriptor desc = { utf8name };
  desc.value = value;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceValue(
    const char* utf8name,
    Value value,
    napi_property_attributes attributes = napi_default) {
  napi_property_descriptor desc = { utf8name };
  desc.value = value;
  desc.attributes = static_cast<napi_property_attributes>(
    attributes | napi_instance_property);
  return desc;
}

template <typename T>
inline void ObjectWrap<T>::ConstructorCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  bool isConstructCall;
  napi_status status = napi_is_construct_call(env, info, &isConstructCall);
  if (status != napi_ok) return;

  if (!isConstructCall) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  T* instance;
  napi_value wrapper;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    instance = callbackData->constructorCallback(callbackInfo);
    instance->_wrapper = callbackInfo.This().MakePersistent();
    wrapper = callbackInfo.This();
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  status = napi_wrap(env, wrapper, instance, nullptr, nullptr); // TODO: Destructor?
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, wrapper);
  if (status != napi_ok) return;
}

template <typename T>
inline void ObjectWrap<T>::StaticVoidMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    callbackData->staticVoidMethodCallback(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }
}

template <typename T>
inline void ObjectWrap<T>::StaticMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    result = callbackData->staticMethodCallback(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  napi_status status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

template <typename T>
inline void ObjectWrap<T>::StaticGetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    result = callbackData->staticGetterCallback(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  napi_status status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

template <typename T>
inline void ObjectWrap<T>::StaticSetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    callbackData->staticSetterCallback(callbackInfo, callbackInfo[0]);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }
}

template <typename T>
inline void ObjectWrap<T>::InstanceVoidMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    T* instance = callbackInfo.This().Unwrap<T>();
    auto cb = callbackData->instanceVoidMethodCallback;
    (instance->*cb)(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }
}

template <typename T>
inline void ObjectWrap<T>::InstanceMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    T* instance = callbackInfo.This().Unwrap<T>();
    auto cb = callbackData->instanceMethodCallback;
    result = (instance->*cb)(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  napi_status status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

template <typename T>
inline void ObjectWrap<T>::InstanceGetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  napi_value result;
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    T* instance = callbackInfo.This().Unwrap<T>();
    auto cb = callbackData->instanceGetterCallback;
    result = (instance->*cb)(callbackInfo);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }

  napi_status status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

template <typename T>
inline void ObjectWrap<T>::InstanceSetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    CallbackData* callbackData = reinterpret_cast<CallbackData*>(callbackInfo.Data());
    T* instance = callbackInfo.This().Unwrap<T>();
    auto cb = callbackData->instanceSetterCallback;
    (instance->*cb)(callbackInfo, callbackInfo[0]);
  }
  catch (const Error& e) {
    if (!Env(env).IsExceptionPending()) {
      e.ThrowAsJavaScriptException();
    }
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// HandleScope class
////////////////////////////////////////////////////////////////////////////////

inline HandleScope::HandleScope(napi_env env, napi_handle_scope scope)
    : _env(env), _scope(scope) {
}

inline HandleScope::HandleScope(Napi::Env env) : _env(env) {
  napi_status status = napi_open_handle_scope(_env, &_scope);
  if (status != napi_ok) throw Env().NewError();
}

inline HandleScope::~HandleScope() {
  napi_close_handle_scope(_env, _scope);
}

inline HandleScope::operator napi_handle_scope() const {
  return _scope;
}

inline Env HandleScope::Env() const {
  return Napi::Env(_env);
}

////////////////////////////////////////////////////////////////////////////////
// EscapableHandleScope class
////////////////////////////////////////////////////////////////////////////////

inline EscapableHandleScope::EscapableHandleScope(
  napi_env env, napi_escapable_handle_scope scope) : _env(env), _scope(scope) {
}

inline EscapableHandleScope::EscapableHandleScope(Napi::Env env) : _env(env) {
  napi_status status = napi_open_escapable_handle_scope(_env, &_scope);
  if (status != napi_ok) throw Env().NewError();
}

inline EscapableHandleScope::~EscapableHandleScope() {
  napi_close_escapable_handle_scope(_env, _scope);
}

inline EscapableHandleScope::operator napi_escapable_handle_scope() const {
  return _scope;
}

inline Env EscapableHandleScope::Env() const {
  return Napi::Env(_env);
}

inline Value EscapableHandleScope::Escape(Value escapee) {
  napi_value result;
  napi_status status = napi_escape_handle(_env, _scope, escapee, &result);
  if (status != napi_ok) throw Env().NewError();
  return Value(_env, result);
}


////////////////////////////////////////////////////////////////////////////////
// Callback class
////////////////////////////////////////////////////////////////////////////////

inline Callback::Callback() : _env(nullptr), _handle(nullptr) {
}

inline Callback::Callback(napi_env env, napi_value fn) : _env(env), _handle(nullptr) {
  HandleScope scope = HandleScope(Env());

  napi_value obj;
  napi_status status = napi_create_object(env, &obj);
  if (status != napi_ok) throw Env().NewError();

  status = napi_set_element(env, obj, _kCallbackIndex, fn);
  if (status != napi_ok) throw Env().NewError();

  status = napi_create_persistent(env, obj, &_handle);
  if (status != napi_ok) throw Env().NewError();
}

inline Callback::Callback(Function fn) : Callback(fn.Env(), fn) {
}

inline Callback::~Callback() {
  if (_handle == nullptr) {
    return;
  }

  napi_release_persistent(_env, _handle);
}

inline bool Callback::operator ==(const Callback &other) const {
  HandleScope scope = HandleScope(Env());
  return this->GetFunction().StrictEquals(other.GetFunction());
}

inline bool Callback::operator !=(const Callback &other) const {
  return !this->operator==(other);
}

inline Value Callback::operator *() const {
  return GetFunction();
}

inline Value Callback::operator ()(Object& recv, const std::vector<Value>& args) const {
  return Call(recv, args);
}

inline Value Callback::operator ()(const std::vector<Value>& args) const {
  return Call(args);
}

inline Env Callback::Env() const {
  return Napi::Env(_env);
}

inline bool Callback::IsEmpty() const {
  return GetFunction().Type() == napi_undefined;
}

inline Value Callback::GetFunction() const {
  EscapableHandleScope scope = EscapableHandleScope(Env());

  napi_value obj;
  napi_status status = napi_get_persistent_value(_env, _handle, &obj);
  if (status != napi_ok) throw Env().NewError();

  napi_value fn;
  status = napi_get_element(_env, obj, _kCallbackIndex, &fn);
  if (status != napi_ok) throw Env().NewError();

  return scope.Escape(Value(_env, fn));
}

inline void Callback::SetFunction(const Value& fn) {
  EscapableHandleScope scope = EscapableHandleScope(Env());

  napi_value obj;
  napi_status status = napi_get_persistent_value(_env, _handle, &obj);
  if (status != napi_ok) throw Env().NewError();

  status = napi_set_element(_env, obj, _kCallbackIndex, fn);
  if (status != napi_ok) throw Env().NewError();
}

inline Value Callback::Call(const std::vector<Value>& args) const {
  Object global = Env().Global();
  return Call(global, args);
}

inline Value Callback::Call(Object& recv, const std::vector<Value>& args) const {
  EscapableHandleScope scope = EscapableHandleScope(Env());

  // Convert args from Value to napi_value.
  std::vector<napi_value> argv;
  argv.reserve(args.size());
  for (size_t i = 0; i < args.size(); i++) {
    argv.push_back(args[i]);
  }

  napi_value cb;
  napi_status status = napi_make_callback(
    _env,
    recv,
    GetFunction(),
    argv.size(),
    argv.data(),
    &cb);
  if (status != napi_ok) throw Env().NewError();

  return scope.Escape(Value(_env, cb));
}

} // namespace Napi

#endif // SRC_NAPI_INL_H_
