#include <thread.h>
#include <guard.h>
#include <lib/hash.h>
#include <slab.h>
#include <processor.h>
#include <memory.h>

static struct hash thread_hash;
static struct spinlock thread_hash_lock = SPINLOCK_INIT;
static _Atomic unsigned long _next_id = 0;

static void _thread_ref_put(void *obj)
{
	struct thread *thr = obj;
	spinlock_acquire(&thread_hash_lock);
	hash_delete(&thread_hash, &thr->id, sizeof(thr->id));
	spinlock_release(&thread_hash_lock);
	slab_release(obj);
}

static void _thread_ref_init(void *obj)
{
	struct thread *thr = obj;
	spinlock_acquire(&thread_hash_lock);
	hash_insert(&thread_hash, &thr->id, sizeof(thr->id), &thr->elem, thr);
	spinlock_release(&thread_hash_lock);
}

static struct refcalls thread_ref_calls = REFCALLS(_thread_ref_init, _thread_ref_put);

static void _thread_init(void *obj)
{
	struct thread *thr = obj;
	thr->id = ++_next_id;
	ref_init(&thr->ref, thr, &thread_ref_calls);
}

static void _thread_create(void *obj)
{
	struct thread *thr = obj;
	thr->kernel_stack = (void *)mm_virtual_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, false);
	_thread_init(obj);
}
static void _thread_release(void *obj) { printk("thread released\n"); }
static void _thread_destroy(void *obj) { }

static struct slab_allocator so_thread = SLAB_ALLOCATOR(sizeof(struct thread), 64, _thread_create, _thread_init, _thread_release, _thread_destroy);

struct thread *thread_lookup(unsigned long id)
{
	spinlock_guard(&thread_hash_lock);
	struct thread *thr = hash_lookup(&thread_hash, &id, sizeof(id));
	if(thr && ref_try_get_nonzero(&thr->ref))
		return thr;
	return NULL;
}

struct thread *thread_create(void *jump, void *arg)
{
	struct thread *thread = slab_alloc(&so_thread);
	arch_thread_start(thread, jump, arg);
	thread->state = THREADSTATE_RUNNING;
	return thread;
}

__initializer static void _thread_hash_init(void)
{
	hash_create(&thread_hash, HASH_LOCKLESS, 1024);
}

void thread_initialize_processor(struct processor *proc)
{
	arch_thread_initialize(&proc->idle_thread);
	proc->idle_thread.processor = proc;
	proc->idle_thread.kernel_stack = proc->initial_stack;
}

_Noreturn void thread_exit(void)
{
	processor_disable_preempt();
	current_thread->state = THREADSTATE_DEAD;
	workqueue_insert(&current_thread->processor->wq, &current_thread->del_task, (void (*)(void *))ref_put, &current_thread->ref);
	processor_enable_preempt();
	schedule();
	panic("unreachable");
}

