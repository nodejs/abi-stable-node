#ifndef SRC_NODE_API_HELPERS_H_
#define SRC_NAPI_H_

////////////////////////////////////////////////////////////////////////////////
// NAPI C++ Wrapper Classes
//
// These classes wrap the "NAPI" ABI-stable C APIs for Node.js, providing a
// C++ object model and C++ exception-handling semantics with low overhead.
// The wrappers are all header-only so that they do not affect the ABI.
////////////////////////////////////////////////////////////////////////////////

#include "node_jsvmapi.h"
#include "node_asyncapi.h"
#include <string>
#include <vector>

namespace Napi {

  class Value;
  class Boolean;
  class Number;
  class String;
  class Object;
  class Array;
  class Function;
  class Buffer;
  class Error;
  class Persistent;
  class PropertyDescriptor;
  class CallbackInfo;

  // Functions exposed to JavaScript must conform to one these callback signatures.
  // (See ObjectWrap<T> for callbacks used when wrapping entire classes.)
  typedef void (*VoidFunctionCallback)(const CallbackInfo& info);
  typedef Value (*FunctionCallback)(const CallbackInfo& info);

  /*
   * Environment for NAPI operations. (In V8 this corresponds to an Isolate.)
   */
  class Env {
  public:
    explicit Env(napi_env env);

    operator napi_env() const;

    static Env Current();

    Object Global() const;
    Value Undefined() const;
    Value Null() const;

    Error NewError();
    Error NewError(const char* message);
    Error NewTypeError(const char* message);
    Buffer NewBuffer(char* data, size_t size);
    Buffer CopyBuffer(const char* data, size_t size);
    Number NewNumber(double val);
    Boolean NewBoolean(bool val);
    String NewStringUtf8(const char* val, int length = -1);
    String NewStringUtf16(const char16_t* val, int length = -1);
    Array NewArray();
    Array NewArray(int length);
    Object NewObject();
    Function NewFunction(VoidFunctionCallback cb,
                         const char* utf8name = nullptr,
                         void* data = nullptr);
    Function NewFunction(FunctionCallback cb,
                         const char* utf8name = nullptr,
                         void* data = nullptr);

    bool IsExceptionPending() const;
    void Throw(Value error);
    void ThrowError(const char* message);
    void ThrowTypeError(const char* message);

  private:
    napi_env _env;

    static void VoidFunctionCallbackWrapper(napi_env env, napi_callback_info info);
    static void FunctionCallbackWrapper(napi_env env, napi_callback_info info);

    typedef struct {
      union {
        VoidFunctionCallback voidFunctionCallback;
        FunctionCallback functionCallback;
      };
      void* data;
    } CallbackData;
  };

  class PropertyName {
  public:
    PropertyName(napi_env env, napi_propertyname name);
    PropertyName(napi_env env, const char* utf8name);

    operator napi_propertyname() const;

    Env Env() const;

  private:
    napi_env _env;
    napi_propertyname _name;
  };

  /*
   * Represents a JavaScript value of unknown type.
   *
   * For type-specific operations, convert to one of the Value subclasses using a
   * To* or As* method. The To* methods do type coercion; the As* methods do not.
   */
  class Value {
  public:
    Value();
    Value(napi_env env, napi_value value);

    operator napi_value() const;
    bool operator ==(const Value& other) const;
    bool operator !=(const Value& other) const;
    bool StrictEquals(const Value& other) const;
    Persistent MakePersistent() const;

    Env Env() const;

    napi_valuetype Type() const;
    bool IsUndefined() const;
    bool IsNull() const;
    bool IsBoolean() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsSymbol() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsFunction() const;
    bool IsBuffer() const;

    Boolean AsBoolean() const;
    Number AsNumber() const;
    String AsString() const;
    Object AsObject() const;
    Array AsArray() const;
    Function AsFunction() const;
    Buffer AsBuffer() const;

    Boolean ToBoolean() const;
    Number ToNumber() const;
    String ToString() const;
    Object ToObject() const;

  protected:
    napi_env _env;
    napi_value _value;
  };

  class Boolean : public Value {
  public:
    Boolean();
    Boolean(napi_env env, napi_value value);
    operator bool() const;
    bool Value() const;
  };

  class Number : public Value {
  public:
    Number();
    Number(napi_env env, napi_value value);
    operator int32_t() const;
    operator uint32_t() const;
    operator int64_t() const;
    operator float() const;
    operator double() const;
    int32_t Int32Value() const;
    uint32_t Uint32Value() const;
    int64_t Int64Value() const;
    float FloatValue() const;
    double DoubleValue() const;
  };

  class String : public Value {
  public:
    String();
    String(napi_env env, napi_value value);
    operator std::string() const;
    operator std::u16string() const;
    std::string Utf8Value() const;
    std::u16string Utf16Value() const;
  };

  class Object : public Value {
  public:
    Object();
    Object(napi_env env, napi_value value);
    Value operator [](const char* name) const;
    bool Has(const PropertyName& name) const;
    bool Has(const char* utf8name) const;
    Value Get(const PropertyName& name) const;
    Value Get(const char* utf8name) const;
    void Set(const PropertyName& name, const Value& value);
    void Set(const char* utf8name, const Value& value);
    void Set(const char* utf8name, const char* utf8value);
    void Set(const char* utf8name, bool boolValue);
    void Set(const char* utf8name, double numberValue);
    void DefineProperty(const PropertyDescriptor& property);
    void DefineProperties(const std::vector<PropertyDescriptor>& properties);
    bool InstanceOf(const Value& constructor) const;
    template<typename T> T* Unwrap() const;
  };

  class Array : public Value {
  public:
    Array();
    Array(napi_env env, napi_value value);
    Value operator [](uint32_t index) const;
    uint32_t Length() const;
    bool Has(uint32_t index) const;
    Value Get(uint32_t index) const;
    void Set(uint32_t index, const Value& value);
    void Set(uint32_t index, const char* utf8value);
    void Set(uint32_t index, bool boolValue);
    void Set(uint32_t index, double numberValue);
  };

  class Function : public Value {
  public:
    Function();
    Function(napi_env env, napi_value value);
    Value operator ()(const std::vector<Value>& args) const;
    Value operator ()(Object& recv, const std::vector<Value>& args) const;
    Value Call(const std::vector<Value>& args) const;
    Value Call(Object& recv, const std::vector<Value>& args) const;
    Value MakeCallback(const std::vector<Value>& args) const;
    Value MakeCallback(Value& recv, const std::vector<Value>& args) const;
    Object New(const std::vector<Napi::Value>& args);
  };

  class Buffer : public Value {
  public:
    Buffer();
    Buffer(napi_env env, napi_value value);
    size_t Length() const;
    char* Data() const;
  };

  /*
   * The NAPI Error class wraps a JavaScript Error object in a way that enables it
   * to traverse a C++ stack and be thrown and caught as a C++ exception.
   *
   * If a NAPI API call fails without executing any JavaScript code (for example due
   * to an invalid argument), then the NAPI wrapper automatically converts and throws
   * the error as a C++ exception of type Napi::Error.
   *
   * If a JavaScript function called by C++ code via NAPI throws a JavaScript exception,
   * then the NAPI wrapper automatically converts and throws it as a C++ exception of type
   * Napi::Error.
   *
   * If a C++ exception of type Napi::Error escapes from a NAPI C++ callback, then
   * the NAPI wrapper automatically converts and throws it as a JavaScript exception.
   *
   * Catching a C++ exception of type Napi::Error also clears the JavaScript exception.
   * Of course it may be then re-thrown, which restores the JavaScript exception.
   *
   * Example 1 - Throwing an exception:
   *
   *   Napi::Env env = ...
   *   throw env.NewError("Example exception");
   *   // Following C++ statements will not be executed.
   *   // The exception will bubble up as a C++ exception of type Napi::Error,
   *   // until it is either caught while still in C++, or else automatically
   *   // re-thrown as a JavaScript exception when the callback returns to JavaScript.
   *
   * Example 2 - Ignoring a NAPI exception:
   *
   *   Napi::Function jsFunctionThatThrows = someObj.AsFunction();
   *   jsFunctionThatThrows({ arg1, arg2 });
   *   // Following C++ statements will not be executed.
   *   // The exception will bubble up as a C++ exception of type Napi::Error,
   *   // until it is either caught while still in C++, or else automatically
   *   // re-thrown as a JavaScript exception when the callback returns to JavaScript.
   *
   * Example 3 - Handling a NAPI exception:
   *
   *   Napi::Function jsFunctionThatThrows = someObj.AsFunction();
   *   try {
   *      jsFunctionThatThrows({ arg1, arg2 });
   *   }
   *   catch (const Napi::Error& e) {
   *     cerr << "Caught JavaScript exception: " + e.what();
   *     // Since the exception was caught here, it will not be re-thrown as
   *     // a JavaScript exception.
   *   }
   */
  class Error : public Value, public std::exception {
  public:
    Error();
    Error(napi_env env, napi_value value);

    std::string Message() const;
    void ThrowAsJavaScriptException() const;

    const char* what() const override;

  private:
    std::string _message;
  };

  class Persistent {
  public:
    Persistent();
    Persistent(napi_env env, napi_persistent persistent);
    ~Persistent();

    // A persistent instance can be moved but cannot be copied.
    Persistent(Persistent&& other);
    Persistent& operator =(Persistent&& other);
    Persistent& operator =(Persistent&) = delete;
    Persistent(const Persistent&) = delete;

    operator napi_persistent() const;

    Env Env() const;
    Value Value() const;

  private:
    napi_env _env;
    napi_persistent _persistent;
  };

  class CallbackInfo {
  public:
    CallbackInfo(napi_env env, napi_callback_info info);

    Env Env() const;
    int Length() const;
    const Value operator [](int index) const;
    Object This() const;
    void* Data() const;

  private:
    napi_env _env;
    napi_value _this;
    std::vector<napi_value> _args;
    void* _data;
  };

  class PropertyDescriptor {
  public:
    PropertyDescriptor(napi_property_descriptor desc) : _desc(desc) {}

    operator napi_property_descriptor&() { return _desc; }
    operator const napi_property_descriptor&() const { return _desc; }

  private:
    napi_property_descriptor _desc;
  };

  /*
  * Property descriptor for use with ObjectWrap<T>::DefineClass(). This is different from
  * the standalone PropertyDescriptor because it is specific to each ObjectWrap<T>
  * subclass. This prevents using descriptors from a different class when defining a new class
  * (preventing the callbacks from having incorrect `this` pointers).
  */
  template <typename T>
  class ClassPropertyDescriptor {
  public:
    ClassPropertyDescriptor(napi_property_descriptor desc) : _desc(desc) {}

    operator napi_property_descriptor&() { return _desc; }
    operator const napi_property_descriptor&() const { return _desc; }

  private:
    napi_property_descriptor _desc;
  };

  /*
   * Base class to be extended by C++ classes exposed to JavaScript.
   * Each C++ class instance gets "wrapped" by a JavaScript object.
   *
   * At initialization time, the DefineClass() method must be used to
   * hook up the accessor and method callbacks. It takes a list of
   * property descriptors, which can be constructed via the various
   * static methods on the base class.
   *
   * Example:
   *
   *   class Example: public Napi::ObjectWrap<Example> {
   *   public:
   *     static void Initialize(Napi::Env& env, Napi::Object& target) {
   *       Napi::Function constructor = DefineClass(env, "Example", New, {
   *         InstanceAccessor("value", &GetValue, &SetValue),
   *         InstanceMethod("doSomething", &DoSomething),
   *       });
   *       target.Set("Example", constructor);
   *     }
   *
   *     static Example* New(const Napi::CallbackInfo& info) {
   *       return new Example();
   *     }
   *
   *     Napi::Value GetValue(const Napi::CallbackInfo& info);
   *     void SetValue(const Napi::CallbackInfo& info, const Napi::Value& value);
   *     Napi::Value DoSomething(const Napi::CallbackInfo& info);
   *   }
   */
  template <typename T>
  class ObjectWrap {
  public:
    ObjectWrap();

    Object Wrapper() const;

    // Methods exposed to JavaScript must conform to one of these callback signatures.
    typedef T* (*ConstructorCallback)(const CallbackInfo& info);
    typedef void (*StaticVoidMethodCallback)(const CallbackInfo& info);
    typedef Value (*StaticMethodCallback)(const CallbackInfo& info);
    typedef Value (*StaticGetterCallback)(const CallbackInfo& info);
    typedef void (*StaticSetterCallback)(const CallbackInfo& info, const Value& value);
    typedef void (T::*InstanceVoidMethodCallback)(const CallbackInfo& info);
    typedef Value (T::*InstanceMethodCallback)(const CallbackInfo& info);
    typedef Value (T::*InstanceGetterCallback)(const CallbackInfo& info);
    typedef void (T::*InstanceSetterCallback)(const CallbackInfo& info, const Value& value);

    typedef ClassPropertyDescriptor<T> PropertyDescriptor;

    static Function DefineClass(Env env,
                                const char* utf8name,
                                ConstructorCallback constructor,
                                const std::vector<PropertyDescriptor>& properties,
                                void* data = nullptr);
    static PropertyDescriptor StaticMethod(const char* utf8name,
                                           StaticVoidMethodCallback method,
                                           napi_property_attributes attributes = napi_default,
                                           void* data = nullptr);
    static PropertyDescriptor StaticMethod(const char* utf8name,
                                           StaticMethodCallback method,
                                           napi_property_attributes attributes = napi_default,
                                           void* data = nullptr);
    static PropertyDescriptor StaticAccessor(const char* utf8name,
                                             StaticGetterCallback getter,
                                             StaticSetterCallback setter,
                                             napi_property_attributes attributes = napi_default,
                                             void* data = nullptr);
    static PropertyDescriptor InstanceMethod(const char* utf8name,
                                             InstanceVoidMethodCallback method,
                                             napi_property_attributes attributes = napi_default,
                                             void* data = nullptr);
    static PropertyDescriptor InstanceMethod(const char* utf8name,
                                             InstanceMethodCallback method,
                                             napi_property_attributes attributes = napi_default,
                                             void* data = nullptr);
    static PropertyDescriptor InstanceAccessor(const char* utf8name,
                                               InstanceGetterCallback getter,
                                               InstanceSetterCallback setter,
                                               napi_property_attributes attributes = napi_default,
                                               void* data = nullptr);
    static PropertyDescriptor StaticValue(const char* utf8name,
                                          Value value,
                                          napi_property_attributes attributes = napi_default);
    static PropertyDescriptor InstanceValue(const char* utf8name,
                                            Value value,
                                            napi_property_attributes attributes = napi_default);

  private:
    Persistent _wrapper;

    static void ConstructorCallbackWrapper(napi_env env, napi_callback_info info);
    static void StaticVoidMethodCallbackWrapper(napi_env env, napi_callback_info info);
    static void StaticMethodCallbackWrapper(napi_env env, napi_callback_info info);
    static void StaticGetterCallbackWrapper(napi_env env, napi_callback_info info);
    static void StaticSetterCallbackWrapper(napi_env env, napi_callback_info info);
    static void InstanceVoidMethodCallbackWrapper(napi_env env, napi_callback_info info);
    static void InstanceMethodCallbackWrapper(napi_env env, napi_callback_info info);
    static void InstanceGetterCallbackWrapper(napi_env env, napi_callback_info info);
    static void InstanceSetterCallbackWrapper(napi_env env, napi_callback_info info);

    typedef struct {
      union {
        ConstructorCallback constructorCallback;
        StaticVoidMethodCallback staticVoidMethodCallback;
        StaticMethodCallback staticMethodCallback;
        struct {
          StaticGetterCallback staticGetterCallback;
          StaticSetterCallback staticSetterCallabck;
        };
        InstanceVoidMethodCallback instanceVoidMethodCallback;
        InstanceMethodCallback instanceMethodCallback;
        struct {
          InstanceGetterCallback instanceGetterCallback;
          InstanceSetterCallback instanceSetterCallback;
        };
      };
      void* data;
    } CallbackData;
  };

  class HandleScope {
  public:
    HandleScope(napi_env env, napi_handle_scope scope);
    explicit HandleScope(Napi::Env env);
    ~HandleScope();

    operator napi_handle_scope() const;

    Env Env() const;

  private:
    napi_env _env;
    napi_handle_scope _scope;
  };

  class EscapableHandleScope {
  public:
    EscapableHandleScope(napi_env env, napi_escapable_handle_scope scope);
    explicit EscapableHandleScope(Napi::Env env);
    ~EscapableHandleScope();

    operator napi_escapable_handle_scope() const;

    Env Env() const;
    Value Escape(Value escapee);

  private:
    napi_env _env;
    napi_escapable_handle_scope _scope;
  };

  class Callback {
  public:
    Callback();
    Callback(napi_env env, napi_value fn);
    explicit Callback(Function fn);
    ~Callback();

    bool operator ==(const Callback &other) const;
    bool operator !=(const Callback &other) const;

    Value operator *() const;
    Value operator ()(Object& recv, const std::vector<Value>& args) const;
    Value operator ()(const std::vector<Value>& args) const;

    Env Env() const;
    bool IsEmpty() const;
    Value GetFunction() const;
    void SetFunction(const Value& fn);

    Value Call(const std::vector<Value>& args) const;
    Value Call(Object& recv, const std::vector<Value>& args) const;

  private:
    napi_env _env;
    napi_persistent _handle;
    static const uint32_t _kCallbackIndex = 0;
  };

} // namespace Napi

// Inline implementations of all the above class methods are included here.
#include "napi-inl.h"

#endif // SRC_NAPI_H_
