#ifndef SRC_NODE_API_HELPERS_H_
#define SRC_NODE_API_HELPERS_H_

////////////////////////////////////////////////////////////////////////////////
// Nan-like Helpers
////////////////////////////////////////////////////////////////////////////////
// Must not have symbol exports to achieve clean C opaque API/ABI
//
// TODO(ianhall): This should all move into its own header file and perhaps be
// optional since it contains a lot of inline code and brings in a lot of header
// dependencies.
////////////////////////////////////////////////////////////////////////////////
#include "node_jsvmapi.h"
#include "node_asyncapi.h"
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define NAPI_METHOD(name)                                                      \
  void name(napi_env env, napi_callback_info info)
#define NAPI_GETTER(name) NAPI_METHOD(name)
#define NAPI_SETTER(name) NAPI_METHOD(name)

#define NAPI_MODULE_INIT(name)                                                 \
  void name(napi_env env, napi_value exports, napi_value module)

// This is taken from NAN and is the C++11 version.
// TODO(ianhall): Support pre-C++11 compilation?
#define NAPI_DISALLOW_ASSIGN(CLASS) void operator=(const CLASS&) = delete;
#define NAPI_DISALLOW_COPY(CLASS) CLASS(const CLASS&) = delete;
#define NAPI_DISALLOW_MOVE(CLASS)                                              \
  CLASS(CLASS&) = delete;                                                      \
  void operator=(CLASS&) = delete;

#define NAPI_DISALLOW_ASSIGN_COPY_MOVE(CLASS)                                  \
  NAPI_DISALLOW_ASSIGN(CLASS)                                                  \
  NAPI_DISALLOW_COPY(CLASS)                                                    \
  NAPI_DISALLOW_MOVE(CLASS)

namespace Napi {
  // Napi::New helpers
  inline napi_value New(napi_env env) {
    return napi_new_empty_value(env);
  }
  inline napi_value New(napi_env env, bool val) {
    return napi_create_boolean(env, val);
  }
  inline napi_value New(napi_env env, int val) {
    return napi_create_number(env, val);
  }
  inline napi_value New(napi_env env, double val) {
    return napi_create_number(env, val);
  }
  inline napi_value New(napi_env env, const char* val) {
    return napi_create_string(env, val);
  }
  inline napi_value New(napi_env env, const char* val, size_t len) {
    return napi_create_string_with_length(env, val, len);
  }
  inline napi_value New(napi_env env, napi_persistent p) {
    return napi_get_persistent_value(env, p);
  }
  inline napi_value NewSymbol(napi_env env, const char* val) {
    return napi_create_symbol(env, val);
  }
  inline napi_value NewObject(napi_env env) {
    return napi_create_object(env);
  }
  inline napi_value NewArray(napi_env env) {
    return napi_create_array(env);
  }
  inline napi_value NewArray(napi_env env, int len) {
    return napi_create_array_with_length(env, len);
  }
  inline napi_value NewFunction(napi_env env, napi_callback cb) {
    return napi_create_function(env, cb, NULL);
  }


  inline napi_value Undefined(napi_env env) {
    return napi_get_undefined_(env);
  }

  inline napi_value Null(napi_env env) {
    return napi_get_null(env);
  }

  // Error Helpers
  inline napi_value Error(napi_env env, const char* errmsg) {
    return napi_create_error(env, napi_create_string(env, errmsg));
  }
  inline napi_value Error(napi_env env, napi_value errmsg) {
    return napi_create_error(env, errmsg);
  }

  inline napi_value TypeError(napi_env env, const char* errmsg) {
    return napi_create_type_error(env, napi_create_string(env, errmsg));
  }
  inline napi_value TypeError(napi_env env, napi_value errmsg) {
    return napi_create_type_error(env, errmsg);
  }

  // Error Helpers
  inline void ThrowError(napi_env env, char* errmsg) {
    napi_throw_error(env, errmsg);
  }
  inline void ThrowError(napi_env env, napi_value errmsg) {
    napi_throw(env, napi_create_error(env, errmsg));
  }
  inline void ThrowTypeError(napi_env env, char* errmsg) {
    napi_throw_type_error(env, errmsg);
  }
  inline void ThrowTypeError(napi_env env, napi_value errmsg) {
    napi_throw(env, napi_create_type_error(env, errmsg));
  }

  // Get helpers
  inline napi_value GetPropertyNames(napi_env env, napi_value obj) {
    return napi_get_propertynames(env, obj);
  }

  inline napi_value Get(napi_env env, napi_value obj, napi_propertyname key ) {
    return napi_get_property(env, obj, key);
  }
  inline napi_value Get(napi_env env, napi_value obj, const char* name) {
    napi_propertyname key = napi_property_name(env, name);
    return napi_get_property(env, obj, key);
  }
  inline napi_value Get(napi_env env, napi_value obj, napi_value name) {
    napi_propertyname key = reinterpret_cast<napi_propertyname>(name);
    return napi_get_property(env, obj, key);
  }
  inline napi_value Get(napi_env env, napi_value arr, uint32_t i ) {
    return napi_get_element(env, arr, i);
  }

  // Set  helpers
  inline void Set(napi_env env, napi_value obj,
                  napi_propertyname key, napi_value val) {
    napi_set_property(env, obj, key, val);
  }
  inline void Set(napi_env env, napi_value obj,
                  const char* name, napi_value val) {
    napi_propertyname key = napi_property_name(env, name);
    napi_set_property(env, obj, key, val);
  }
  inline void Set(napi_env env, napi_value obj,
                  napi_value name, napi_value val) {
    napi_propertyname key = reinterpret_cast<napi_propertyname>(name);
    napi_set_property(env, obj, key, val);
  }
  inline void Set(napi_env env, napi_value arr, uint32_t i, napi_value val) {
    napi_set_element(env, arr, i, val);
  }

  // Type check helpers
  inline bool IsEmpty(napi_env env, napi_value v) {
    return napi_is_empty(env, v);
  }
  inline bool IsNumber(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_number;
  }
  inline bool IsString(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_string;
  }
  inline bool IsFunction(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_function;
  }
  inline bool IsObject(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_object;
  }
  inline bool IsBoolean(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_boolean;
  }
  inline bool IsUndefined(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_undefined;
  }
  inline bool IsSymbol(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_symbol;
  }
  inline bool IsArray(napi_env env, napi_value val) {
    return napi_is_array(env, val);
  }
  inline bool IsRegExp(napi_env env, napi_value val) {
    return napi_is_regexp(env, val);
  }
  inline bool IsDate(napi_env env, napi_value val) {
    return napi_is_date(env, val);
  }
  inline bool IsNull(napi_env env, napi_value val) {
    return napi_get_type_of_value(env, val) == napi_null;
  }

  inline bool HasInstance(napi_env env, napi_value tpl, napi_value obj) {
    return napi_instanceof(env, obj, tpl);
  }

  inline napi_value CopyBuffer(napi_env env, const char* buf, uint32_t size) {
    return napi_buffer_copy(env, buf, size);
  }

  inline napi_value StringConcat(napi_env env,
                                 napi_value str1, napi_value str2) {
    return napi_string_concat(env, str1, str2);
  }

  inline napi_value MakeCallback(
      napi_env env,
      napi_value target,
      napi_value func,
      int argc,
      napi_value* argv
  ) {
    return napi_make_callback(env, target, func, argc, argv);
  }

  // Length helpers
  inline int Length(napi_env env, napi_value val) {
    assert(napi_get_type_of_value(env, val) == napi_array);
    return napi_get_array_length(env, val);
  }

  inline int Length(napi_env env, napi_callback_info info) {
    return napi_get_cb_args_length(env, info);
  }

  // Conversion helpers
  inline void To(napi_env env, napi_value v, int32_t& o) {
    o = napi_get_value_int32(env, v);
  }

  inline void To(napi_env env, napi_value v, uint32_t& o) {
    o = napi_get_value_uint32(env, v);
  }

  inline void To(napi_env env, napi_value v, int64_t& o) {
    o = napi_get_value_int64(env, v);
  }

  inline void To(napi_env env, napi_value v, bool& o) {
    o = napi_get_value_bool(env, v);
  }

  inline void To(napi_env env, napi_value v, double& o) {
    o = napi_get_number_from_value (env, v);
  }

  template<typename T>
  inline T To(napi_env env, napi_value v) {
    T ret;
    To(env, v, ret);
    return ret;
  }

  inline napi_value ToString(napi_env env, napi_value v) {
    return napi_coerce_to_string(env, v);
  }
  inline napi_value ToObject(napi_env env, napi_value v) {
    return napi_coerce_to_object(env, v);
  }

  inline bool Equals(napi_env env, napi_value lhs, napi_value rhs) {
    return napi_strict_equals(env, lhs, rhs);
  }

  inline std::vector<napi_value> GetArguments(napi_env env,
                                              napi_callback_info info) {
    int len = napi_get_cb_args_length(env, info);
    std::vector<napi_value> args;
    args.reserve(len);
    napi_get_cb_args(env, info, args.data(), args.size());
    return args;
  }

  // RAII HandleScope helpers
  // Mirror Nan versions for easy conversion
  // Ensure scopes are closed in the correct order on the stack
  class HandleScope {
   public:
    HandleScope() {
      env = napi_get_current_env();
      scope = napi_open_handle_scope(env);
    }
    explicit HandleScope(napi_env e) : env(e) {
      scope = napi_open_handle_scope(env);
    }
    ~HandleScope() {
      napi_close_handle_scope(env, scope);
    }

   private:
    napi_env env;
    napi_handle_scope scope;
  };

  class EscapableHandleScope {
   public:
    EscapableHandleScope() {
      env = napi_get_current_env();
      scope = napi_open_escapable_handle_scope(env);
    }
    explicit EscapableHandleScope(napi_env e) : env(e) {
      scope = napi_open_escapable_handle_scope(e);
    }
    ~EscapableHandleScope() {
      napi_close_escapable_handle_scope(env, scope);
    }

    napi_value Escape(napi_value escapee) {
      return napi_escape_handle(env, scope, escapee);
    }

   private:
    napi_env env;
    napi_escapable_handle_scope scope;
  };

  class Utf8String {
   public:
    inline explicit Utf8String(napi_value from) :
        length_(0), str_(str_st_) {
      if (from != NULL) {
        napi_env env = napi_get_current_env();
        napi_value string = napi_coerce_to_string(env, from);
        if (string != NULL) {
          size_t len = 3 * napi_get_string_length(env, string) + 1;
          assert(len <= INT_MAX);
          if (len > sizeof(str_st_)) {
            str_ = new char[len];
            assert(str_ != 0);
          }
          length_ = napi_get_string_utf8(env, string, str_,
                                         static_cast<int>(len));
          str_[length_] = '\0';
        }
      }
    }

    inline int length() const {
      return length_;
    }

    inline char* operator*() { return str_; }
    inline const char* operator*() const { return str_; }

    inline ~Utf8String() {
      if (str_ != str_st_) {
        delete [] str_;
      }
    }

   private:
    NAPI_DISALLOW_ASSIGN_COPY_MOVE(Utf8String)

    int length_;
    char *str_;
    char str_st_[1024];
  };

  class ObjectWrap {
   public:
    ObjectWrap() {
      napi_env env = napi_get_current_env();
      handle_ = napi_create_persistent(env, nullptr);
      refs_ = 0;
    }

    virtual ~ObjectWrap() {
      napi_env env = napi_get_current_env();
      napi_release_persistent(env, persistent());
    }

    template <class T>
    static inline T* Unwrap(napi_value object) {
      napi_env env = napi_get_current_env();
      assert(!napi_is_empty(env, object));
      assert(Napi::Has(env, object, "internal_field"));
      void* ptr = Napi::Get(env, object, "internal_field");
      ObjectWrap* wrap = static_cast<ObjectWrap*>(ptr);
      return static_cast<T*>(wrap);
    }

    inline napi_value handle() {
      napi_env env = napi_get_current_env();
      return napi_get_persistent_value(env, persistent());
    }

    inline napi_persistent persistent() {
      return handle_;
    }

   protected:
    inline void Wrap(napi_value object) {
      napi_env env = napi_get_current_env();
      assert(napi_is_persistent_empty(env, persistent()));
      assert(Napi::Has(env, object, "internal_field"));
      Napi::Set(env, object, "internal_field", this);
      napi_release_persistent(env, persistent());
      handle_ = napi_create_persistent(env, object);
      napi_persistent_make_weak(env, persistent(), this);
    }

    /* Ref() marks the object as being attached to an event loop.
     * Refed objects will not be garbage collected, even if
     * all references are lost.
     */
    virtual void Ref() {
      napi_env env = napi_get_current_env();
      assert(!napi_is_persistent_empty(env, persistent()));
      napi_persistent_clear_weak(env, persistent());
      refs_++;
    }

    /* Unref() marks an object as detached from the event loop.  This is its
     * default state.  When an object with a "weak" reference changes from
     * attached to detached state it will be freed. Be careful not to access
     * the object after making this call as it might be gone!
     * (A "weak reference" means an object that only has a
     * persistent handle.)
     *
     * DO NOT CALL THIS FROM DESTRUCTOR
     */
    virtual void Unref() {
      napi_env env = napi_get_current_env();
      assert(!napi_is_persistent_empty(env, persistent()));
      assert(refs_ > 0);
      if (--refs_ == 0)
        napi_persistent_make_weak(env, persistent(), this);
    }

    int refs_;

   private:
    napi_persistent handle_;
};


  // TODO(ianhall): This class uses napi_get_current_env() extensively
  // does that make sense? will this behave correctly when passed around
  // workers?  Is there a good reason to have napi_env?
  // Perhaps the C++ layer shields the user from napi_env and behaves
  // like Nan, always using the current one?
  class Callback {
   public:
    Callback() {
      napi_env env = napi_get_current_env();
      HandleScope scope(env);
      napi_value obj = napi_create_object(env);
      handle = napi_create_persistent(env, obj);
    }

    explicit Callback(napi_value fn) {
      napi_env env = napi_get_current_env();
      HandleScope scope(env);
      napi_value obj = napi_create_object(env);
      handle = napi_create_persistent(env, obj);
      SetFunction(fn);
    }

    ~Callback() {
      if (handle == NULL) {
        return;
      }
      napi_release_persistent(napi_get_current_env(), handle);
    }

    bool operator==(const Callback &other) const {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_value a = napi_get_element(env,
                                      napi_get_persistent_value(env, handle),
                                      kCallbackIndex);
      napi_value b = napi_get_element(env,
                                      napi_get_persistent_value(env,
                                      other.handle), kCallbackIndex);
      return napi_strict_equals(env, a, b);
    }

    bool operator!=(const Callback &other) const {
      return !this->operator==(other);
    }

    inline
    napi_value operator*() const { return this->GetFunction(); }

    inline napi_value operator()(
        napi_value target,
        int argc = 0,
        napi_value argv[] = 0) const {
      return this->Call(target, argc, argv);
    }

    inline napi_value operator()(
        int argc = 0,
        napi_value argv[] = 0) const {
      return this->Call(argc, argv);
    }

    inline bool IsFunction() {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_value fn =
          napi_get_element(env,
              napi_get_persistent_value(env, handle), kCallbackIndex);
      return napi_function == napi_get_type_of_value(env, fn);
    }

    inline void SetFunction(napi_value fn) {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_set_element(env, napi_get_persistent_value(env, handle),
                       kCallbackIndex, fn);
    }

    inline napi_value GetFunction() const {
      EscapableHandleScope scope;
      napi_env env = napi_get_current_env();
      return scope.Escape(
          napi_get_element(env, napi_get_persistent_value(env, handle),
                           kCallbackIndex));
    }

    inline bool IsEmpty() const {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_value fn =
          napi_get_element(env,
              napi_get_persistent_value(env, handle), kCallbackIndex);
      return napi_undefined == napi_get_type_of_value(env, fn);
    }

    inline napi_value
    Call(napi_value target,
        int argc,
        napi_value argv[]) const {
      return Call_(target, argc, argv);
    }

    inline napi_value
    Call(int argc, napi_value argv[]) const {
      napi_env env = napi_get_current_env();
      return Call_(napi_get_global_scope(env), argc, argv);
    }

   private:
    NAPI_DISALLOW_ASSIGN_COPY_MOVE(Callback)
    napi_persistent handle;
    static const uint32_t kCallbackIndex = 0;

    napi_value Call_(napi_value target,
                   int argc,
                   napi_value argv[]) const {
      EscapableHandleScope scope;
      napi_env env = napi_get_current_env();

      napi_value callback =
          napi_get_element(env, napi_get_persistent_value(env, handle),
                           kCallbackIndex);
      return scope.Escape(napi_make_callback(
        env,
        target,
        callback,
        argc,
        argv));
    }
  };


  // TODO(ianhall): This class uses napi_get_current_env() extensively
  // See comment above on class Callback
  /* abstract */ class AsyncWorker {
   public:
    explicit AsyncWorker(Callback *callback_)
        : callback(callback_), errmsg_(NULL) {
      request = napi_create_async_work();
      napi_env env = napi_get_current_env();

      HandleScope scope;
      napi_value obj = napi_create_object(env);
      persistentHandle = napi_create_persistent(env, obj);
    }

    virtual ~AsyncWorker() {
      HandleScope scope;

      if (persistentHandle != NULL) {
        napi_env env = napi_get_current_env();
        napi_release_persistent(env, persistentHandle);
        persistentHandle = NULL;
      }
      delete callback;
      delete[] errmsg_;

      napi_delete_async_work(request);
    }

    virtual void WorkComplete() {
      HandleScope scope;

      if (errmsg_ == NULL)
        HandleOKCallback();
      else
        HandleErrorCallback();
      delete callback;
      callback = NULL;
    }

    inline void SaveToPersistent(
        const char *key, napi_value value) {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_propertyname pnKey = napi_property_name(env, key);
      napi_set_property(env, napi_get_persistent_value(env, persistentHandle),
                        pnKey, value);
    }

    inline void SaveToPersistent(
        napi_propertyname key, napi_value value) {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_set_property(env, napi_get_persistent_value(env, persistentHandle),
                        key, value);
    }

    inline void SaveToPersistent(
        uint32_t index, napi_value value) {
      HandleScope scope;
      napi_env env = napi_get_current_env();
      napi_set_element(env, napi_get_persistent_value(env, persistentHandle),
                       index, value);
    }

    inline napi_value GetFromPersistent(const char *key) const {
      EscapableHandleScope scope;
      napi_env env = napi_get_current_env();
      napi_propertyname pnKey = napi_property_name(env, key);
      return scope.Escape(
          napi_get_property(env,
              napi_get_persistent_value(env, persistentHandle), pnKey));
    }

    inline napi_value
    GetFromPersistent(napi_propertyname key) const {
      EscapableHandleScope scope;
      napi_env env = napi_get_current_env();
      return scope.Escape(
          napi_get_property(env,
              napi_get_persistent_value(env, persistentHandle), key));
    }

    inline napi_value GetFromPersistent(uint32_t index) const {
      EscapableHandleScope scope;
      napi_env env = napi_get_current_env();
      return scope.Escape(
          napi_get_element(env,
              napi_get_persistent_value(env, persistentHandle), index));
    }

    virtual void Execute() = 0;

    napi_work request;

    static void CallExecute(void* this_pointer){
      AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
      self->Execute();
    }

    static void CallWorkComplete(void* this_pointer) {
      AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
      self->WorkComplete();
    }

   protected:
    napi_persistent persistentHandle;
    Callback *callback;

    virtual void HandleOKCallback() {
      callback->Call(0, NULL);
    }

    virtual void HandleErrorCallback() {
      HandleScope scope;
      napi_env env = napi_get_current_env();

      napi_value argv[] = {
        napi_create_error(env, napi_create_string(env, ErrorMessage()))
      };
      callback->Call(1, argv);
    }

    void SetErrorMessage(const char *msg) {
      delete[] errmsg_;

      size_t size = strlen(msg) + 1;
      errmsg_ = new char[size];
      memcpy(errmsg_, msg, size);
    }

    const char* ErrorMessage() const {
      return errmsg_;
    }

   private:
    NAPI_DISALLOW_ASSIGN_COPY_MOVE(AsyncWorker)
    char *errmsg_;
  };

  inline void AsyncQueueWorker(AsyncWorker* worker) {
    napi_work req = worker->request;
    napi_async_set_data(req, static_cast<void*>(worker));
    napi_async_set_execute(req, &AsyncWorker::CallExecute);
    napi_async_set_complete(req, &AsyncWorker::CallWorkComplete);
    napi_async_queue_worker(req);
  }
} // namespace Napi


#endif // SRC_NODE_API_HELPERS_H_
