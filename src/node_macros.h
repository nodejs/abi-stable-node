#ifndef SRC_NODE_MACROS_H_
#define SRC_NODE_MACROS_H_

#ifdef _WIN32
#ifndef BUILDING_NODE_EXTENSION
#define NODE_EXTERN __declspec(dllexport)
#else
#define NODE_EXTERN __declspec(dllimport)
#endif
#else
#define NODE_EXTERN /* nothing */
#endif

#ifdef _WIN32
#define NODE_MODULE_EXPORT __declspec(dllexport)
#else
#define NODE_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#endif  // SRC_NODE_MACROS_H_
