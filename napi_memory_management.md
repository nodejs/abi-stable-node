# Memory Management

As N-API calls are made, references to objects in the heap for the underlying
vm may be returned as ```napi_values```.  These references must hold the
objects 'live' until they are no longer required by the native code,
otherwise the objects could be collected before the native code was
finished using them.

As objects references are returned they are associated with a
'scope'. The lifespan for the default scope is tied to the lifespan
of the native method call. The result is that, by default, references
remain valid and the objects associated with these references will be
held live for the lifespan of the native method call.   

In many cases, however, it is necessary that the references remain valid for
either a shorter or longer lifespan than that of the native method.
The sections which follow describe the N-API functions than can be used
to change the reference lifespan from the default.

## Making reference lifespan shorter than that of native method

It is often necessary to make the lifespan for references shorter than
the lifespan of a native method. For example, consider a native method
that has a loop which iterates through the elements in a large array:

```C
  for (int i = 0; i < 1000000; i++) {
    napi_value result;
    napi_get_element(e object, i, &result);
    // do something with element
  }
```

This would result in a large number of references being created, consuming
substantial resources. In addition, even through the native code could only
use the most recent reference, all of the associated objects would also be
kept alive, even though the native method was no longer able to access them.

To handle this case, N-API provides the ability to establish a new 'scope' to
which newly created references will be associated.  Once those references
are no longer required, the scope can be 'closed' and any references associated
with the scope are invalidated.  The methods available to open/close scopes are:

```C
NODE_EXTERN napi_status napi_open_handle_scope(napi_env e,
                                               napi_handle_scope* result);
NODE_EXTERN napi_status napi_close_handle_scope(napi_env e,
                                                napi_handle_scope scope);
```

N-API only supports a single nested hiearchy of scopes.  There is only one
active scope at any time, and all new references will be associated with that
scope while it is active.  Scopes must be closed in the reverse order from
which they are opened.  In addition, all scopes created within a native method
must be closed before returning from that method.

Taking the earlier example, adding calls to ```napi_open_handle_scope()``` and
```napi_close_handle_scope()```  would ensure that at most a single reference
is valid throughout the execution of the loop:

```
for (int i = 0; i < 1000000; i++) {napi_
  napi_handle_scope scope;
  napi_open_handle_scope(env, &scope);
  napi_value result;
  napi_get_element(e object, i, &result);
  // do something with element
  napi_close_handle_scope(env, scope);
}
```

## Making reference lifespan longer than that of native method

In some cases an addon will need to be able to create and reference objects
with a lifespan longer than that of a single native method invocation. For
example, if you want to create a constructor and later use that constructor
in a request to creates instances, you will need to be able to reference
the constructor object across many different instance creation requests. This
would not be possible with a normal references returned as a napi_value as
described in the earlier section.  The lifespan or normal references are
managed by scopes and all scopes must be closed before the end of a native
method.

N-API provides methods to create persistent references.  Each persistent
reference has an associated count with a value of 0 or higher. The count
determines if the reference will keep the corresponding object live.  
references with a count of 0 do not prevent the object from being collected,
and are often called 'weak' references. Any count greater than 0 will prevent
the object from being collected.

references can be created with an initial reference count.  The count can
then be modified through ```napi_reference_addref()``` and
```napi_reference_release()```.  If an object is collected while the count
for a reference is 0, all subsequent calls to
get the object associated with the reference (```napi_get_reference_value()```)
will return NULL for the returned napi_value.  An attempt to call
```napi_reference_addref()``` for a reference whose object has been collected,
will result in an error.

references must be deleted once they are no longer required by the addon. When
a reference is deleted it will no longer prevent the corresponding object from
being collected.  Failure to delete a persistent reference will result in
a 'memory leak' with both the native memory for the persistent reference and
the corresponding object in heap being retained forever.

There can be multiple persistent references created which refer to the same
object, each of which will either keep the object live or not based on its
individual count.

The methods for managing persistent references are:

```C
NODE_EXTERN napi_status napi_create_reference(napi_env e,
                                              napi_value value,
                                              int initial_refcount,
                                              napi_ref* result);

NODE_EXTERN napi_status napi_delete_reference(napi_env e, napi_ref ref);

NODE_EXTERN napi_status napi_reference_addref(napi_env e,
                                              napi_ref ref,
                                              int* result);

NODE_EXTERN napi_status napi_reference_release(napi_env e,
                                               napi_ref ref,
                                               int* result);

NODE_EXTERN napi_status napi_get_reference_value(napi_env e,
                                                 napi_ref ref,
                                                 napi_value* result);
```
