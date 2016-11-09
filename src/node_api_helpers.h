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

#define NAPI_METHOD(name)                                                      \
  void name(napi_env env, napi_func_cb_info info)

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

    virtual void Destroy() {
        delete this;
    }

    static void CallExecute(void* this_pointer){
      AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
      self->Execute();
    }

    static void CallWorkComplete(void* this_pointer) {
      AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
      self->WorkComplete();
    }

    static void CallDestroy(void* this_pointer) {
      AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
      self->Destroy();
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
    napi_async_set_destroy(req, &AsyncWorker::CallDestroy);
    napi_async_queue_worker(req);
  }
} // namespace Napi


#endif // SRC_NODE_API_HELPERS_H_
