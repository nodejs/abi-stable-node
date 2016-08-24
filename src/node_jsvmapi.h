/*******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * This is a rough proof of concept not intended for real world usage.
 * It is currently far from a sufficiently completed work.
 *
 *  - The API is not complete nor agreed upon.
 *  - The API is not yet analyzed for performance.
 *  - Performance is expected to suffer with the usage of opaque types currently
 *    requiring all operations to go across the DLL boundary, i.e. no inlining
 *    even for operations such as asking for the type of a napi_value or retrieving a
 *    function napi_callback's arguments.
 *  - The V8 implementation of the API is roughly hacked together with no
 *    attention paid to error handling or fault tolerance.
 *  - Error handling in general is not included in the API at this time.
 *
 ******************************************************************************/
#ifndef SRC_NODE_JSVMAPI_H_
#define SRC_NODE_JSVMAPI_H_

#include "node_jsvmapi_types.h"

// TODO(ianhall): We're using C linkage for the API but we're also using the
// bool type in these exports.  Is that safe and stable?
extern "C" {
enum napi_valuetype {
    // ES6 types (corresponds to typeof)
    napi_undefined,
    napi_null,
    napi_boolean,
    napi_number,
    napi_string,
    napi_symbol,
    napi_object,
    napi_function,
};

// Environment
NODE_EXTERN napi_env napi_get_current_env();


// Getters for defined singletons
NODE_EXTERN napi_value napi_get_undefined_(napi_env e);
NODE_EXTERN napi_value napi_get_null(napi_env e);
NODE_EXTERN napi_value napi_get_false(napi_env e);
NODE_EXTERN napi_value napi_get_true(napi_env e);
NODE_EXTERN napi_value napi_get_global_scope(napi_env e);


// Methods to create Primitive types/Objects
NODE_EXTERN napi_value napi_create_object(napi_env e);
NODE_EXTERN napi_value napi_create_array(napi_env e);
NODE_EXTERN napi_value napi_create_array_with_length(napi_env e, int length);
NODE_EXTERN napi_value napi_create_number(napi_env e, double val);
NODE_EXTERN napi_value napi_create_string(napi_env e, const char*);
NODE_EXTERN napi_value napi_create_string_with_length(napi_env e, const char*,
                                                               size_t length);
NODE_EXTERN napi_value napi_create_boolean(napi_env e, bool b);
NODE_EXTERN napi_value napi_create_symbol(napi_env e, const char* s = NULL);
NODE_EXTERN napi_value napi_create_function(napi_env e, napi_callback cbinfo);
NODE_EXTERN napi_value napi_create_error(napi_env e, napi_value msg);
NODE_EXTERN napi_value napi_create_type_error(napi_env e, napi_value msg);


// Methods to get the the native napi_value from Primitive type
NODE_EXTERN napi_valuetype napi_get_type_of_value(napi_env e, napi_value v);
NODE_EXTERN double napi_get_number_from_value(napi_env e, napi_value v);
NODE_EXTERN int napi_get_string_from_value(napi_env e, napi_value v,
                                           char* buf, const int buf_size);
NODE_EXTERN int32_t napi_get_value_int32(napi_env e, napi_value v);
NODE_EXTERN uint32_t napi_get_value_uint32(napi_env e, napi_value v);
NODE_EXTERN int64_t napi_get_value_int64(napi_env e, napi_value v);
NODE_EXTERN bool napi_get_value_bool(napi_env e, napi_value v);

NODE_EXTERN int napi_get_string_length(napi_env e, napi_value v);
NODE_EXTERN int napi_get_string_utf8_length(napi_env e, napi_value v);
NODE_EXTERN int napi_get_string_utf8(napi_env e, napi_value v,
                                     char* buf, int bufsize);


// Methods to coerce values
// These APIs may execute user script
NODE_EXTERN napi_value napi_coerce_to_object(napi_env e, napi_value v);
NODE_EXTERN napi_value napi_coerce_to_string(napi_env e, napi_value v);


// Methods to work with Objects
NODE_EXTERN napi_value napi_get_prototype(napi_env e, napi_value object);
NODE_EXTERN napi_propertyname napi_property_name(napi_env e,
                                                 const char* utf8name);
NODE_EXTERN napi_value napi_get_propertynames(napi_env e, napi_value object);
NODE_EXTERN void napi_set_property(napi_env e, napi_value object,
                                   napi_propertyname name, napi_value v);
NODE_EXTERN bool napi_has_property(napi_env e, napi_value object,
                                   napi_propertyname name);
NODE_EXTERN napi_value napi_get_property(napi_env e, napi_value object,
                                         napi_propertyname name);
NODE_EXTERN void napi_set_element(napi_env e, napi_value object,
                                  uint32_t i, napi_value v);
NODE_EXTERN bool napi_has_element(napi_env e, napi_value object, uint32_t i);
NODE_EXTERN napi_value napi_get_element(napi_env e,
                                        napi_value object, uint32_t i);


// Methods to work with Arrays
NODE_EXTERN bool napi_is_array(napi_env e, napi_value v);
NODE_EXTERN uint32_t napi_get_array_length(napi_env e, napi_value v);

// Methods to compare values
NODE_EXTERN bool napi_strict_equals(napi_env e, napi_value lhs, napi_value rhs);


// Methods to work with Functions
NODE_EXTERN void napi_set_function_name(napi_env e, napi_value func,
                                        napi_propertyname napi_value);
NODE_EXTERN napi_value napi_call_function(napi_env e, napi_value scope,
                                          napi_value func,
                                          int argc, napi_value* argv);
NODE_EXTERN napi_value napi_new_instance(napi_env e, napi_value cons,
                                         int argc, napi_value* argv);

// Napi version of node::MakeCallback(...)
NODE_EXTERN napi_value napi_make_callback(napi_env e, napi_value recv,
                                          napi_value func,
                                          int argc, napi_value* argv);


// Methods to work with napi_callbacks
NODE_EXTERN int napi_get_cb_args_length(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN void napi_get_cb_args(napi_env e, napi_func_cb_info cbinfo,
                                  napi_value* buffer, size_t bufferlength);
NODE_EXTERN napi_value napi_get_cb_this(napi_env e, napi_func_cb_info cbinfo);
// V8 concept; see note in .cc file
NODE_EXTERN napi_value napi_get_cb_holder(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN bool napi_is_construct_call(napi_env e, napi_func_cb_info cbinfo);
NODE_EXTERN void napi_set_return_value(napi_env e, napi_func_cb_info cbinfo,
                                       napi_value v);


// Methods to support ObjectWrap
NODE_EXTERN napi_value napi_create_constructor_for_wrap(napi_env e,
                                                        napi_callback cb);
NODE_EXTERN void napi_wrap(napi_env e, napi_value jsObject, void* nativeObj,
                           napi_destruct* napi_destructor,
                           napi_persistent* handle);
NODE_EXTERN void* napi_unwrap(napi_env e, napi_value jsObject);

NODE_EXTERN napi_value napi_create_constructor_for_wrap_with_methods(
  napi_env e,
  napi_callback cb,
  char* utf8name,
  int methodcount,
  napi_method_descriptor* methods);

// Methods to control object lifespan
NODE_EXTERN napi_persistent napi_create_persistent(napi_env e, napi_value v);
NODE_EXTERN void napi_release_persistent(napi_env e, napi_persistent p);
NODE_EXTERN napi_value napi_get_persistent_value(napi_env e, napi_persistent p);
NODE_EXTERN napi_handle_scope napi_open_handle_scope(napi_env e);
NODE_EXTERN void napi_close_handle_scope(napi_env e, napi_handle_scope s);
NODE_EXTERN napi_escapable_handle_scope
                napi_open_escapable_handle_scope(napi_env e);
NODE_EXTERN void napi_close_escapable_handle_scope(
                                             napi_env e,
                                             napi_escapable_handle_scope s);
NODE_EXTERN napi_value napi_escape_handle(napi_env e,
                                          napi_escapable_handle_scope s,
                                          napi_value v);


// Methods to support error handling
NODE_EXTERN void napi_throw(napi_env e, napi_value error);
NODE_EXTERN void napi_throw_error(napi_env e, char* msg);
NODE_EXTERN void napi_throw_type_error(napi_env e, char* msg);

// TODO(ianhall): APIs for handling try catch semantics need serious
// design analysis, this is just a quick hack
// One thing to note is that v8::TryCatch relies on the current
// stack pointer position, so it cannot be used
// with a wrapper class hack like we're doing for handle scopes.
typedef void (*napi_try_callback)(napi_env e, void* data);
typedef void (*napi_catch_callback)(napi_env e, void* data);
NODE_EXTERN bool napi_try_catch(napi_env e, napi_try_callback t,
                                napi_catch_callback c, void* data);


// Methods to provide node::Buffer functionality with napi types
NODE_EXTERN napi_value napi_buffer_copy(napi_env e,
                                        const char* data, uint32_t size);
NODE_EXTERN bool napi_buffer_has_instance(napi_env e, napi_value v);
NODE_EXTERN char* napi_buffer_data(napi_env e, napi_value v);
NODE_EXTERN size_t napi_buffer_length(napi_env e, napi_value v);

}  // extern "C"

////////////////////////////////////////////////////////////////////////////////
// Nan-like Helpers
////////////////////////////////////////////////////////////////////////////////
// Must not have symbol exports to achieve clean C opaque API/ABI
//
// TODO(ianhall): This should all move into its own header file and perhaps be
// optional since it contains a lot of inline code and brings in a lot of header
// dependencies.
////////////////////////////////////////////////////////////////////////////////
#include <limits.h>
#include <string.h>
#include <uv.h>

#define NAPI_METHOD(name)                                                      \
    void name(napi_env env, napi_func_cb_info info)

// This is taken from NAN and is the C++11 version.
// TODO(ianhall): Support pre-C++11 compilation?
#define NAPI_DISALLOW_ASSIGN(CLASS) void operator=(const CLASS&) = delete;
#define NAPI_DISALLOW_COPY(CLASS) CLASS(const CLASS&) = delete;
#define NAPI_DISALLOW_MOVE(CLASS)                                              \
    CLASS(CLASS&) = delete;                                                   \
    void operator=(CLASS&) = delete;

#define NAPI_DISALLOW_ASSIGN_COPY_MOVE(CLASS)                                  \
    NAPI_DISALLOW_ASSIGN(CLASS)                                                \
    NAPI_DISALLOW_COPY(CLASS)                                                  \
    NAPI_DISALLOW_MOVE(CLASS)

namespace Napi {
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
      if (from != nullptr) {
        napi_env env = napi_get_current_env();
        napi_value string = napi_coerce_to_string(env, from);
        if (string != nullptr) {
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
      if (handle == nullptr) {
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
        : callback(callback_), errmsg_(nullptr) {
      request.data = this;
      napi_env env = napi_get_current_env();

      HandleScope scope;
      napi_value obj = napi_create_object(env);
      persistentHandle = napi_create_persistent(env, obj);
    }

    virtual ~AsyncWorker() {
      HandleScope scope;

      if (persistentHandle != nullptr) {
        napi_env env = napi_get_current_env();
        napi_release_persistent(env, persistentHandle);
        persistentHandle = nullptr;
      }
      delete callback;
      delete[] errmsg_;
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

    uv_work_t request;

    virtual void Destroy() {
        delete this;
    }

  protected:
    napi_persistent persistentHandle;
    Callback *callback;

    virtual void HandleOKCallback() {
      callback->Call(0, nullptr);
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

  inline void AsyncExecute(uv_work_t* req) {
    AsyncWorker *worker = static_cast<AsyncWorker*>(req->data);
    worker->Execute();
  }

  inline void AsyncExecuteComplete(uv_work_t* req) {
    AsyncWorker* worker = static_cast<AsyncWorker*>(req->data);
    worker->WorkComplete();
    worker->Destroy();
  }

  inline void AsyncQueueWorker(AsyncWorker* worker) {
    uv_queue_work(
      uv_default_loop(),
      &worker->request,
      AsyncExecute,
      reinterpret_cast<uv_after_work_cb>(AsyncExecuteComplete));
  }
}  // namespace Napi

////////////////////////////////////////////////////////////////////////////////
// WILL GO AWAY (these can't be extern "C" because they work with C++ types)
////////////////////////////////////////////////////////////////////////////////
NODE_EXTERN v8::Local<v8::Value> V8LocalValue(napi_value v);
NODE_EXTERN napi_value JsValue(v8::Local<v8::Value> v);

NODE_EXTERN v8::Persistent<v8::Value>* V8PersistentValue(napi_persistent p);

#endif  // SRC_NODE_JSVMAPI_H__
