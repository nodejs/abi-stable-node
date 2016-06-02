/*******************************************************************************
 * Experimental prototype for demonstrating VM agnostic and ABI stable API
 * for native modules to use instead of using Nan and V8 APIs directly.
 *
 * This is an rough proof of concept not intended for real world usage.
 * It is currently far from a sufficiently completed work.
 *
 *  - The API is not complete nor agreed upon.
 *  - The API is not yet analyzed for performance.
 *  - Performance is expected to suffer with the usage of opaque types currently
 *    requiring all operations to go across the DLL boundary, i.e. no inlining
 *    even for operations such as asking for the type of a value or retrieving a
 *    function callback's arguments.
 *  - The V8 implementation of the API is roughly hacked together with no
 *    attention paid to error handling or fault tolerance.
 *  - Error handling in general is not included in the API at this time.
 *
 ******************************************************************************/
#include "node_jsvmapi.h"

namespace node {
namespace js { 

namespace v8impl {

//=== Conversion between V8 Isolate and node::js::env ==========================

    static env JsEnvFromV8Isolate(v8::Isolate* isolate) {
        return static_cast<env>(isolate);
    }

    static v8::Isolate* V8IsolateFromJsEnv(env e) {
        return static_cast<v8::Isolate*>(e);
    }

//=== Conversion between V8 Handles and node::js::value ========================

    // This is assuming v8::Local<> will always be implemented with a single
    // pointer field so that we can pass it around as a void* (maybe we should
    // use intptr_t instead of void*)
    static_assert(sizeof(void*) == sizeof(v8::Local<v8::Value>), "v8::Local<v8::Value> is too large to store in a void*");

    static value JsValueFromV8LocalValue(v8::Local<v8::Value> local) {
        // This is awkward, better way? memcpy but don't want that dependency?
        union U {
            value v;
            v8::Local<v8::Value> l;
            U(v8::Local<v8::Value> _l) : l(_l) { }
        } u(local);
        return u.v;
    };

    static v8::Local<v8::Value> V8LocalValueFromJsValue(value v) {
        // Likewise awkward
        union U {
            value v;
            v8::Local<v8::Value> l;
            U(value _v) : v(_v) { }
        } u(v);
        return u.l;
    }

    static value JsValueFromV8PersistentValue(v8::Isolate *isolate, v8::Persistent<v8::Value> persistent) {
        return JsValueFromV8LocalValue(
            v8::Local<v8::Value>::New(isolate, persistent));
    };

//=== Conversion between V8 FunctionCallbackInfo and ===========================
//=== node::js::FunctionCallbackInfo ===========================================

    static FunctionCallbackInfo JsFunctionCallbackInfoFromV8FunctionCallbackInfo(const v8::FunctionCallbackInfo<v8::Value>* cbinfo) {
        return static_cast<FunctionCallbackInfo>(cbinfo);
    };

    static const v8::FunctionCallbackInfo<v8::Value>* V8FunctionCallbackInfoFromJsFunctionCallbackInfo(FunctionCallbackInfo cbinfo) {
        return static_cast<const v8::FunctionCallbackInfo<v8::Value>*>(cbinfo);
    };

//=== Function callback wrapper ================================================

    // This function callback wrapper implementation is taken from nan
    static const int kDataIndex =           0;
    static const int kFunctionIndex =       1;
    static const int kFunctionFieldCount =  2;

    static void FunctionCallbackWrapper(const v8::FunctionCallbackInfo<v8::Value> &info) {
        v8::Local<v8::Object> obj = info.Data().As<v8::Object>();
        callback cb = reinterpret_cast<callback>(
            reinterpret_cast<intptr_t>(
                obj->GetInternalField(kFunctionIndex).As<v8::External>()->Value()));
        FunctionCallbackInfo cbinfo = JsFunctionCallbackInfoFromV8FunctionCallbackInfo(&info);
        cb(
            v8impl::JsEnvFromV8Isolate(info.GetIsolate()),
            cbinfo);
    }

}  // end of namespace v8impl

value CreateFunction(env e, callback cb) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    // This code is adapted from nan

    // This parameter is unused currently but would probably be useful to
    // expose in the API for client code to provide a data object to be
    // included on callback.
    v8::Local<v8::Value> data = v8::Local<v8::Value>();

    v8::Local<v8::Function> retval;

    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::ObjectTemplate> tpl = v8::ObjectTemplate::New(isolate);
    tpl->SetInternalFieldCount(v8impl::kFunctionFieldCount);
    v8::Local<v8::Object> obj = tpl->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();

    obj->SetInternalField(
        v8impl::kFunctionIndex
        , v8::External::New(isolate, reinterpret_cast<void *>(cb)));
    v8::Local<v8::Value> val = v8::Local<v8::Value>::New(isolate, data);

    if (!val.IsEmpty()) {
        obj->SetInternalField(v8impl::kDataIndex, val);
    }

    retval = scope.Escape(v8::Function::New(isolate
        , v8impl::FunctionCallbackWrapper
        , obj));

    return v8impl::JsValueFromV8LocalValue(retval);
}

void SetReturnValue(env e, FunctionCallbackInfo cbinfo, value v) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);
    info->GetReturnValue().Set(val);
}

propertyname PropertyName(env e, const char* utf8name) {
    v8::Local<v8::String> namestring = v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), utf8name, v8::NewStringType::kInternalized).ToLocalChecked();
    return static_cast<propertyname>(v8impl::JsValueFromV8LocalValue(namestring));
}

void SetProperty(env e, value o, propertyname k, value v) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    v8::Local<v8::Object> obj = v8impl::V8LocalValueFromJsValue(o)->ToObject();
    v8::Local<v8::Value> key = v8impl::V8LocalValueFromJsValue(k);
    v8::Local<v8::Value> val = v8impl::V8LocalValueFromJsValue(v);

    obj->Set(key, val);

    // This implementation is missing a lot of details, notably error
    // handling on invalid inputs and regarding what happens in the
    // Set operation (error thrown, key is invalid, the bool return
    // value of Set)
}

value CreateString(env e, const char* s) {
    return v8impl::JsValueFromV8LocalValue(
        v8::String::NewFromUtf8(v8impl::V8IsolateFromJsEnv(e), s));
}

value CreateNumber(env e, double v) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Number::New(v8impl::V8IsolateFromJsEnv(e), v));
}

value CreateTypeError(env, value msg) {
    return v8impl::JsValueFromV8LocalValue(
        v8::Exception::TypeError(
            v8impl::V8LocalValueFromJsValue(msg).As<v8::String>()));
}

valuetype GetTypeOfValue(value vv) {
    v8::Local<v8::Value> v = v8impl::V8LocalValueFromJsValue(vv);

    if (v->IsNumber()) {
        return valuetype::Number;
    }
    else if (v->IsString()) {
        return valuetype::String;
    }
    else if (v->IsFunction()) {
        // This test has to come before IsObject because IsFunction
        // implies IsObject
        return valuetype::Function;
    }
    else if (v->IsObject()) {
        return valuetype::Object;
    }
    else if (v->IsBoolean()) {
        return valuetype::Boolean;
    }
    else if (v->IsUndefined()) {
        return valuetype::Undefined;
    }
    else if (v->IsSymbol()) {
        return valuetype::Symbol;
    }
    else if (v->IsNull()) {
        return valuetype::Null;
    }
    else {
        return valuetype::Object; // Is this correct?
    }
}

int GetCallbackArgsLength(FunctionCallbackInfo cbinfo) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);
    return info->Length();
}

// copy encoded arguments into provided buffer or return direct pointer to
// encoded arguments array?
void GetCallbackArgs(FunctionCallbackInfo cbinfo, value* buffer, size_t bufferlength) {
    const v8::FunctionCallbackInfo<v8::Value> *info = v8impl::V8FunctionCallbackInfoFromJsFunctionCallbackInfo(cbinfo);

    int i = 0;
    // size_t appropriate for the buffer length parameter?
    // Probably this API is not the way to go.
    int min = (int)bufferlength < info->Length() ? (int)bufferlength : info->Length();

    for (; i < min; i += 1) {
        buffer[i] = v8impl::JsValueFromV8LocalValue((*info)[i]);
    }

    if (i < (int)bufferlength) {
        value undefined = v8impl::JsValueFromV8LocalValue(v8::Undefined(v8::Isolate::GetCurrent()));
        for (; i < (int)bufferlength; i += 1) {
            buffer[i] = undefined;
        }
    }
}

void ThrowError(env e, value error) {
    v8::Isolate *isolate = v8impl::V8IsolateFromJsEnv(e);

    isolate->ThrowException(
        v8impl::V8LocalValueFromJsValue(error));
    // any VM calls after this point and before returning
    // to the javascript invoker will fail
}

double GetNumberFromValue(value v) {
    return v8impl::V8LocalValueFromJsValue(v)->NumberValue();
}

void WorkaroundNewModuleInit(v8::Local<v8::Object> exports, workaround_init_callback init) {
    init(
        node::js::v8impl::JsEnvFromV8Isolate(v8::Isolate::GetCurrent()),
        node::js::v8impl::JsValueFromV8LocalValue(exports));
}

}  // namespace js
}  // namespace node
