Twizzler System Calls
=====================

All pointers passed to Twizzler system calls are d-ptrs (see Pointer Manipulation).

## sys_ocreate

``` {.c}
#include <twz/sys.h>
int sys_ocreate(int flags, objid_t kuid, objid_t src, objid_t *id);
```

Creates a new object, optionally copying from a source object. If `src` is zero, the new object will
be zeroed (except for the meta-data page). The default metainfo structure will be generated based on
the specified flags and the kuid. If copying from a source object, the meta-data page will contain
the same data as the source object, except for the metainfo data controlled by the `flags` argument.

Valid bits for the `flags` argument are:

* `TWZ_OC_HASHDATA`: Hash the object data as part of ID generation. This flag sets `MIP_HASHDATA`
	in the `p_flags` entry of the metainfo structure. Cannot be used in conjunction with
	`TWZ_OC_DFL_WRITE`.
* `TWZ_OC_ZERONONCE`: Set the nonce to zero (do not randomly generate a nonce). This sets the nonce
  field of the metainfo structure to zero. Can only be used with `TWZ_OC_HASHDATA`.
* `TWZ_OC_DFL_*`: Where * is `WRITE`, `READ`, `EXEC`, or `USE`. Sets the default permissions for the
  new object, setting the associated `MIP_DFL_*` flag in the `p_flags` entry of the metainfo
  structure.
* `TWZ_OC_PERSIST`: Mark the object as persistent.

If src is non-zero, both objects will be set to copy-on-write and will share underlying pages until
necessary to copy them. Objects used as source which are undergoing modification require a
synchronization event (locking, atomic release, etc.) to ensure consistency between updates and
copying for object creation.

### Return Value

Returns 0 on success, and an error code on error. On success, the new object's ID is written into
the `id` argument. The `id` argument is unchanged on failure.

### Errors

* `-EINVAL`: Invalid argument.
* `-EFAULT`: `id` is not a valid pointer.
* `-ENOENT`: The `src` argument is non-zero and the object cannot be found.
* `-ENOMEM`: Not enough memory to fulfill the request.
* `-ENOSPC`: Object is persistent and there is not enough persistent storage.

## sys_odelete

``` {.c}
#include <twz/sys.h>
int sys_odelete(int flags, objid_t id);
```

Deletes the object specified by `id`. New accesses to the object will not be allowed (but current
accesses will be allowed to continue). For example, if the object is mapped in a security context,
access will still be allowed to continue. Once all in-kernel reference counts to the object reach
zero, the object's resources will be freed.

The flags argument is for future expansion, and must currently be set to 0.

### Return Value

Returns 0 on success, and an error code on error.

### Errors

* `-EINVAL`: Invalid argument.
* `-ENOENT`: Could not find object `id`.

## sys_ocfg

``` {.c}
#include <twz/sys.h>
int sys_ocfg(int flags, objid_t id, long cmd, long arg);
```

Configure object `id`. The `flags` argument is for future expansion and must be set to 0. Valid
commands (given by the `cmd` argument) are:

* `TWZ_OCFG_PERSIST`: Mark object as persistent (`arg` = 1) or volatile (`arg` = 0).

### Return Value

Returns 0 on success, error code on error.

### Errors

#### Generic error codes

* `-EINVAL`: Invalid argument.
* `-ENOENT`: Could not find object `id`.

#### `TWZ_OCFG_PERSIST`

* `-ENOSPC`: Not enough persistent storage to fulfill request.
* `-ENOMEM`: Not enough memory to fulfill request.

## sys_attach

``` {.c}
#include <twz/sys.h>
int sys_attach(objid_t parent, objid_t child, int flags, int type);
```

Attach a child KSO to a parent KSO. The parent KSO must already by a functioning KSO, whereas the
child need not be. When attaching, the kernel will consider the child to be a KSO of type `type`
(see Kernel State Objects). If the child is already considered a KSO by the kernel, and it has a
different type than specified by `type`, the call will fail. The `flags` argument is for future
expansion and must be set to 0.

If `parent` is zero, the call has the same effect as if made with `parent` being the ID of the
thread's control object KSO.

The effect of this call depends on the type of KSO of the child:

* `KSO_ROOT`: Invalid; cannot attach a KSO root object to anything.
* `KSO_VIEW`: Changes a thread's view. Parent must be `KSO_THREAD`.
* `KSO_THREAD`: When attaching to a KSO root, this has the effect of starting a thread in the
  by invoking the `FAULT_RESUME` handler in the new thread as specified by the thread's object. The
  view of the new thread will be set to view specified in the thread's control object. `parent` must
  be a `KSO_ROOT`.
* `KSO_SECCTX`: Attaches a security context to a thread. `parent` must be `KSO_THREAD`.

### Return Value

Returns 0 on success, error code on error.

### Errors

* `-EINVAL`: Invalid argument.
* `-ENOENT`: Either child or parent could not be found.
* `-EBUSY`: Child has a different KSO type than specified.
* `-ENOMEM`: Not enough memory to fulfill request.

## sys_detach

``` {.c}
#include <twz/sys.h>
int sys_detach(objid_t parent, objid_t child, int flags, int type);
```

Detach a KSO child from its parent. Unlike `sys_attach`, this function allows delaying the actual
detachment until some time in the future. The `type` argument must match the KSO type of the child.
Similar to attach, if `parent` is zero, it treats the call as if it was made with `parent` being the
thread control object of the current thread.

The `flags` argument specifies when detachment occurs, and _cannot_ be zero. It is built from the
following bits:

* `TWZ_DETACH_ONSYSCALL(s)`: Detach on system call number `s` (eg. `SYS_BECOME`). This is required.
* `TWZ_DETACH_REATTACH`: Detach on entry to the system call specified, and reattach on exit. Cannot
  be used in conjunction with any flags below.
* `TWZ_DETACH_ONENTRY`: Detach on entry to the specified system call.
* `TWZ_DETACH_ONEXIT`: Detach on exit from the specified system call.

Note that multiple calls to detach will _not_ change previous detach specifications; they can only
add. For example, the following calls,
``` {.c}
sys_detach(0, S, TWZ_DETACH_ONSYSCALL(SYS_BECOME) | TWZ_DETACH_ONENTRY);
sys_detach(0, S, TWZ_DETACH_ONSYSCALL(SYS_BECOME) | TWZ_DETACH_REATTACH);
sys_detach(0, S, TWZ_DETACH_ONSYSCALL(SYS_BECOME) | TWZ_DETACH_ONEXIT);
```
will treat the second call as essentially a no-op. The third call will also have no effect, since
the `ONENTRY` status of detachment remains across all three calls.

### Return Value

Returns 0 on success, error code on error.

### Errors

* `-EINVAL`: Invalid argument.
* `-ENOENT`: Either child or parent could not be found.
* `-EBUSY`: Child has a different KSO type than specified.
* `-ENOMEM`: Not enough memory to fulfill request.

## sys_invalidate

``` {.c}
#include <twz/sys.h>
int sys_invalidate(struct sys_invalidate_op ops[], size_t count);
```

Process a number `count` invalidation requests defined by the array `ops`. The array must have at
least as many entries as `count`. An invalidation request will force the kernel to update any cached
state is has derived from the bytes within the range of an object specified for an invalidation
request. For example, updating a view entry may require the kernel to reconstruct page-tables. This
system call is used to tell the kernel to do so.

The `sys_invalidate_op` structure has the following layout:
``` {.c}
struct sys_invalidate_op {
	objid_t id;
	uint64_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
};
```
where `id` specifies the ID of the KSO to invalidate, and `offset` and `length` describe the region of
the KSO object that has changed. After processing an entry, the kernel writes the result into the
`result` field.

The `flags` field can have the following bits set:

* `KSOI_VALID`: The entry is valid. This flag must be set for an entry to be processed.
* `KSOI_CURRENT`: Instead of looking up an object by the `id` field, interpret the `id` field as
  follows:
  * `KSO_CURRENT_VIEW`: Act as if the `id` were the current view of the current thread.
  * `KSO_CURRENT_ATTACHED_SECCTXS`: Act as if this call was made with all attached security contexts
	being invalidated with this range.
  * `KSO_CURRENT_THREAD`: Act as if the `id` were the thread control object of the current thread.

### Return Value

Returns the number of successful invalidations, or an error code. The `result` field is set to 0 on
success or an error code for each valid invalidation operation entry.

### Errors

The call itself can return the following errors:

* `-EINVAL`: Invalid argument.
* `-EFAULT`: The address of `ops` was invalid.

The `result` field in each valid op can take the following error codes:

* `-EINVAL`: Invalid argument.
* `-ENOENT`: Object `id` could not be found.
* `-ENOMEM`: Not enough memory.
* `-EFAULT`: Range referred to bytes outside the object.

## sys_thrd_sync

``` {.c}
#include <twz/sys.h>
int sys_thrd_sync(size_t count, struct sys_thrd_sync_op ops[]);
```

Perform a synchronization operation on a number of sync addresses. A sync address is an address used
by multiple threads (and/or the kernel) to synchronize on. Threads may either sleep on a sync
address or wake sleeping threads on a sync address. This call operates on multiple sync addresses at
once, allowing operations to be specified for each one.

The `sys_thrd_sync_op` structure has the following format:
```{.c}
struct sys_thrd_sync_op {
	long *addr;
	long arg;
	long res;
	uint32_t op;
	uint32_t flags;
	struct timespec timeout;
};
```

Each entry in `ops` refers to one operation on a sync address. The operation is specified by the
`op` entry, and can be:

* `THREAD_SYNC_SLEEP`: Compare the value at `*addr` with `arg`. If they are the same, sleep waiting
  for a wakeup event on `addr`. If they differ, write `THREAD_SYNC_RES_DIFFER` into `res` and do not
  sleep. If `flags` has the `THREAD_SYNC_OPT_TIMEOUT` flag set, then sleep for a time specified by
  `timeout`, or until a wakeup event. If wakeup occurs because of timeout, `res` will contain
  `THREAD_SYNC_RES_TIMEOUT` instead.
* `THREAD_SYNC_WAKE`: Perform a wakeup event on `addr`, waking up any threads sleeping there. Wake
  up at most `arg` threads. If `arg` is negative, wake up all threads. The `timeout` field is
  ignored. Write the number of threads actually woken up into `res`.

The flags field can contain:

* `THREAD_SYNC_OPT_VALID`: Required. Marks an entry as valid.
* `THREAD_SYNC_OPT_TIMEOUT`: Indicates that the timeout field is valid, and a timeout should be
  applied.

Comparisons, sleeping, and wakeups are done atomically.

This function can specify multiple sync addresses at once. Waiting on multiple addresses will only
sleep the thread if _all_ sleep operations sleep. If only one of them does not, the thread will
return immediately. Specifying multiple operations where some are sleeping and some are wakeups
results in these operations being executed as expected, but in an unspecified order. Specifying a
sleep and a wakeup for the same sync address in one call leads to unspecified behavior.

### Return Value

Returns 0 on success, error code on error. The result of an operation is written to the `res` field
for that operation, and contains one of the following depending on the operation.

#### `THREAD_SYNC_SLEEP`

* 0: The thread slept on this sync point, and was not woken up by it.
* `THREAD_SYNC_RES_WAKEUP`: The thread slept on this sync point, and was woken up by it.
* `THREAD_SYNC_RES_DIFFER`: The value at `*addr` was different from the value of `arg`.
* `THREAD_SYNC_RES_TIMEOUT`: A timeout occurred on this operation.
* An error code (negative).

Note that the kernel is only required to set _one_ of the `res` fields for all the operations passed
to it, though it might set more. If the thread was woken spuriously, no fields will be updated, and
the function will return -EINTR.

#### `THREAD_SYNC_WAKE`

* The number of threads woken up (may be 0).
* An error code (negative).

### Errors

Error codes for the function itself:

* `-EINVAL`: Invalid argument.
* `-EFAULT`: `ops` is an invalid pointer.
* `-EINTR`: Thread was interrupted.

Error codes for the `res` field:

* `-EINVAL`: Invalid argument.
* `-EFAULT`: `addr` is an invalid pointer.

## sys_thrd_spawn

``` {.c}
#include <twz/sys.h>
int sys_thrd_spawn(objid_t tid, struct sys_thrd_spawn_args *args, int flags);
```

### Return Value

### Errors

## sys_become

``` {.c}
#include <twz/sys.h>
int sys_become(struct sys_become_args *args);
```

Set thread state to new state as specified by `args`, including instruction pointer, view
(optionally), and registers. The new state is specified by sys_become_args, which is defined as
follows:

``` {.c}
struct sys_become_args {
	objid_t target_view;
	[architecture-specific contents; see Notes]
};
```

The object specified by `args->target_view` is set as the new view for the thread before it returns
to userspace. If `args->target_view` is zero, the thread's view is unchanged.

### Return Value

This function either does not return from the call-site, or it returns an error. A successful
completion of this system call sets all registers to the state specified, thus this function only
returns on failure.

### Errors
* `-EINVAL`: `args` is an invalid pointer.
* `-EFAULT`: The thread would start executing in an invalid address.
* `-ENOENT`: The object specified by `args->target_view` does not exist, if `args->target_view` is
  non-zero.


### Notes

The architecture-specific contents of sys_become_args is specified as follows:

#### x86_64:
``` {.c}
struct sys_become_args {
	[architecture-independent contents]
	uint64_t target_rip;
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};
```


## sys_thrd_ctl

``` {.c}
#include <twz/sys.h>
int sys_thrd_ctl(int op, long arg);
```

### Return Value

### Errors


