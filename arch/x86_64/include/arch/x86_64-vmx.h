#pragma once

#define VMCS_SIZE 0x1000
#define VMX_RC_SWITCHEPT 1
#define INTR_TYPE_HARD_EXCEPTION  (3 << 8) /* processor exception */
#define INTR_INFO_VALID_MASK      (0x80000000)
enum {
	VMEXIT_REASON_CPUID = 0xa,
	VMEXIT_REASON_VMCALL = 0x12,
	VMEXIT_REASON_EPT_VIOLATION = 48,
	VMEXIT_REASON_INVEPT = 50,
};


enum {
	VMCS_GUEST_CS_SEL                 = 0x802,
	VMCS_GUEST_CS_BASE                = 0x6808,
	VMCS_GUEST_CS_LIM                 = 0x4802,
	VMCS_GUEST_CS_ARBYTES             = 0x4816,
	VMCS_GUEST_DS_SEL                 = 0x806,
	VMCS_GUEST_DS_BASE                = 0x680c,
	VMCS_GUEST_DS_LIM                 = 0x4806,
	VMCS_GUEST_DS_ARBYTES             = 0x481a,
	VMCS_GUEST_ES_SEL                 = 0x800,
	VMCS_GUEST_ES_BASE                = 0x6806,
	VMCS_GUEST_ES_LIM                 = 0x4800,
	VMCS_GUEST_ES_ARBYTES             = 0x4814,
	VMCS_GUEST_FS_SEL                 = 0x808,
	VMCS_GUEST_FS_BASE                = 0x680e,
	VMCS_GUEST_FS_LIM                 = 0x4808,
	VMCS_GUEST_FS_ARBYTES             = 0x481c,
	VMCS_GUEST_GS_SEL                 = 0x80a,
	VMCS_GUEST_GS_BASE                = 0x6810,
	VMCS_GUEST_GS_LIM                 = 0x480a,
	VMCS_GUEST_GS_ARBYTES             = 0x481e,
	VMCS_GUEST_SS_SEL                 = 0x804,
	VMCS_GUEST_SS_BASE                = 0x680a,
	VMCS_GUEST_SS_LIM                 = 0x4804,
	VMCS_GUEST_SS_ARBYTES             = 0x4818,
	VMCS_GUEST_TR_SEL                 = 0x80e,
	VMCS_GUEST_TR_BASE                = 0x6814,
	VMCS_GUEST_TR_LIM                 = 0x480e,
	VMCS_GUEST_TR_ARBYTES             = 0x4822,
	VMCS_GUEST_LDTR_SEL               = 0x80c,
	VMCS_GUEST_LDTR_BASE              = 0x6812,
	VMCS_GUEST_LDTR_LIM               = 0x480c,
	VMCS_GUEST_LDTR_ARBYTES           = 0x4820,
	VMCS_GUEST_GDTR_BASE              = 0x6816,
	VMCS_GUEST_GDTR_LIM               = 0x4810,
	VMCS_GUEST_IDTR_LIM               = 0x4812,
	VMCS_GUEST_IDTR_BASE              = 0x6818,
	VMCS_GUEST_RFLAGS                 = 0x6820,
	VMCS_GUEST_RIP                    = 0x681e,
	VMCS_GUEST_RSP                    = 0x681c,
	VMCS_GUEST_CR0                    = 0x6800,
	VMCS_GUEST_CR3                    = 0x6802,
	VMCS_GUEST_CR4                    = 0x6804,
	VMCS_GUEST_EFER                   = 0x2806,
	VMCS_CR4_READ_SHADOW              = 0x6006,
	VMCS_CR0_READ_SHADOW              = 0x6004,
	VMCS_GUEST_ACTIVITY_STATE         = 0x4826,
	VMCS_GUEST_INTRRUPTIBILITY_INFO   = 0x4824,
	VMCS_GUEST_PENDING_DBG_EXCEPTIONS = 0x6822,
	VMCS_GUEST_IA32_DEBUGCTL          = 0x2802,
	VMCS_IO_BITMAP_B                  = 0x2002,
	VMCS_IO_BITMAP_A                  = 0x2000,
	VMCS_LINK_POINTER                 = 0x2800,
	VMCS_PINBASED_CONTROLS            = 0x4000,
	VMCS_PROCBASED_CONTROLS           = 0x4002,
	VMCS_PROCBASED_CONTROLS_SECONDARY = 0x401e,
	VMCS_EXCEPTION_BITMAP             = 0x4004,
	VMCS_PF_ERROR_CODE_MASK           = 0x4006,
	VMCS_PF_ERROR_CODE_MATCH          = 0x4008,
	VMCS_HOST_CR0                     = 0x6c00,
	VMCS_HOST_CR3                     = 0x6c02,
	VMCS_HOST_CR4                     = 0x6c04,
	VMCS_HOST_EFER                    = 0x2c02,
	VMCS_HOST_CS_SEL                  = 0xc02,
	VMCS_HOST_DS_SEL                  = 0xc06,
	VMCS_HOST_ES_SEL                  = 0xc00,
	VMCS_HOST_FS_SEL                  = 0xc08,
	VMCS_HOST_GS_SEL                  = 0xc0a,
	VMCS_HOST_SS_SEL                  = 0xc04,
	VMCS_HOST_TR_SEL                  = 0xc0c,
	VMCS_HOST_GDTR_BASE               = 0x6c0c,
	VMCS_HOST_IDTR_BASE               = 0x6c0e,
	VMCS_HOST_TR_BASE                 = 0x6c0a,
	VMCS_HOST_GS_BASE                 = 0x6c08,
	VMCS_ENTRY_INTR_INFO              = 0x4016,
	VMCS_APIC_VIRT_ADDR               = 0x2012,
	VMCS_TPR_THRESHOLD                = 0x401c,
	VMCS_HOST_RIP                     = 0x6c16,
	VMCS_HOST_RSP                     = 0x6c14,
	VMCS_EPT_PTR                      = 0x201a,
	VMCS_VM_INSTRUCTION_ERROR         = 0x4400,
	VMCS_VM_INSTRUCTION_LENGTH        = 0x440c,
	VMCS_VM_INSTRUCTION_INFO          = 0x440e,
	VMCS_ENTRY_CONTROLS               = 0x4012,
	VMCS_EXIT_CONTROLS                = 0x400c,
	VMCS_EXIT_REASON                  = 0x4402,
	VMCS_EXIT_QUALIFICATION           = 0x6400,
	VMCS_GUEST_LINEAR_ADDR            = 0x640a,
	VMCS_GUEST_PHYSICAL_ADDR          = 0x2400,
	VMCS_VMFUNC_CONTROLS              = 0x2018,
	VMCS_VIRT_EXCEPTION_INFO_ADDR     = 0x202a,
	VMCS_IA32_SYSENTER_CS             = 0x482a,
	VMCS_IA32_HOST_SYSENTER_CS        = 0x4c00,
	VMCS_MSR_BITMAPS_ADDR             = 0x2004,
	VMCS_CR0_MASK                     = 0x6000,
	VMCS_CR4_MASK                     = 0x6002,
	VMCS_GUEST_PHYSICAL_ADDRESS       = 0x2400,
	VMCS_GUEST_LINEAR_ADDRESS         = 0x640a,
	VMCS_EPTP_INDEX                   = 0x0004,
	VMCS_EPTP_LIST                    = 0x2024,
};

#define VM_FUNCTION_SWITCH_EPTP 0

struct ept {
	uintptr_t phys;
	uint64_t *virt;
};

bool x86_64_ept_map(uintptr_t ept_phys, uintptr_t virt, uintptr_t phys, int level, uint64_t flags);
void x86_64_switch_ept(uintptr_t root);
struct processor;
void x86_64_start_vmx(struct processor *proc);

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)
#define PAGE_LARGE   (1ull << 7)
#define EPT_READ 1ull
#define EPT_WRITE 2ull
#define EPT_EXEC (4ull | (1ull << 10)) /* TODO: separate exec_user and exec_kernel */

#define EPT_MEMTYPE_WB (6 << 3)
#define EPT_MEMTYPE_UC (0)
#define EPT_IGNORE_PAT (1 << 6)
#define EPT_LARGEPAGE  (1 << 7)

#define RECUR_ATTR_MASK (EPT_READ | EPT_WRITE | EPT_EXEC)
#define GET_VIRT_TABLE(x) ((uintptr_t *)mm_ptov(((x) & VM_PHYS_MASK)))


