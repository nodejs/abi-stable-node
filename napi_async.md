# Async Helpers

Addon modules often need to leverage async helpers from libuv as part of their
implementation.  This allows them to schedule work to be execute asynchronously
so that their methods can return in advance of the work being completed.  This
is important in order to allow them to avoid blocking overall execution
of the Node.js application.

N-API provides an ABI-stable interface for these
supporting functions which covers the most common asynchronous use cases.

N-API defines the `napi_work` structure which is used to manage
asynchronous wokers. Instances are created/deleted with the following methods:

```C
NAPI_EXTERN napi_work napi_create_async_work();
NAPI_EXTERN void napi_delete_async_work(napi_work w);
```

Once a `napi_work` is created, functions must be provided by the addon
that will be executed when the worker executes, completes its work, or
is destroyed.  These functions are as follows:

```
NAPI_EXTERN void napi_async_set_execute(napi_work w, void (*execute)(void*));
NAPI_EXTERN void napi_async_set_complete(napi_work w, void (*complete)(void*));
NAPI_EXTERN void napi_async_set_destroy(napi_work w, void (*destroy)(void*));
```

When these methods are invoked, the `void*` parameter passed will be the
addon-provided data that can be set using the `napi_async_set_data`
function:

```C
NAPI_EXTERN void napi_async_set_data(napi_work w, void* data);
```

Once created and one or more of the `execute`, `complete` or
`destroy` functions have been set, the async worker can be queued
for execution using the `napi_async_queue_worker` function:

```C
NAPI_EXTERN void napi_async_queue_worker(napi_work w);
```
