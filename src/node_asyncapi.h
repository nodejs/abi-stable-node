#ifndef SRC_NODE_ASYNCAPI_H_
#define SRC_NODE_ASYNCAPI_H_

#include <stdlib.h>
#include "node_asyncapi_types.h"

#ifndef NODE_EXTERN
#ifdef _WIN32
#ifndef BUILDING_NODE_EXTENSION
#define NODE_EXTERN __declspec(dllexport)
#else
#define NODE_EXTERN __declspec(dllimport)
#endif
#else
#define NODE_EXTERN /* nothing */
#endif
#endif

extern "C" {
NODE_EXTERN napi_work napi_create_async_work();
NODE_EXTERN void napi_delete_async_work(napi_work w);
NODE_EXTERN void napi_async_set_data(napi_work w, void* data);
NODE_EXTERN void napi_async_set_execute(napi_work w, void (*execute)(void*));
NODE_EXTERN void napi_async_set_complete(napi_work w, void (*complete)(void*));
NODE_EXTERN void napi_async_set_destroy(napi_work w, void (*destroy)(void*));
NODE_EXTERN void napi_async_queue_worker(napi_work w);
}  // extern "C"

#endif  // SRC_NODE_ASYNCAPI_H_
