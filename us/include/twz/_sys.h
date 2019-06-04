#define SYS_NULL 0
#define SYS_THRD_SPAWN 1
#define SYS_DEBUG_PRINT 2
#define SYS_INVL_KSO 3
#define SYS_ATTACH 4
#define SYS_DETACH 5
#define SYS_BECOME 6
#define SYS_THRD_SYNC 7
#define SYS_OCREATE 8
#define SYS_ODELETE 9
#define SYS_THRD_CTL 10
#define NUM_SYSCALLS 11

struct sys_invalidate_op {
	objid_t id;
	uint64_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
} __packed;

#define KSO_INVL_RES_OK 0
#define KSO_INVL_RES_ERR -1
#define KSOI_VALID 1

#define THREAD_SYNC_SLEEP 0
#define THREAD_SYNC_WAKE 1

/* TODO: arch-dep */
struct sys_become_args {
	objid_t target_view;
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

struct sys_thrd_spawn_args {
	objid_t target_view;
	void (*start_func)(void *); /* thread entry function. */
	void *arg;                  /* argument for entry function. */
	char *stack_base;           /* stack base address. */
	size_t stack_size;
	char *tls_base; /* tls base address. */
	size_t thrd_ctrl;
};

/* TODO: arch-dep */
#define THRD_CTL_SET_FS 1
#define THRD_CTL_SET_GS 2
#define THRD_CTL_SET_IOPL 3
#define THRD_CTL_EXIT 0x100
