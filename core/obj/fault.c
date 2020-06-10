#include <page.h>
#include <pager.h>
#include <processor.h>
#include <secctx.h>
#include <slots.h>
#include <thread.h>
#include <tmpmap.h>

int obj_check_permission(struct object *obj, uint64_t flags)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
		return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
		return 0;
	}
	return secctx_check_permissions((void *)arch_thread_instruction_pointer(), obj, flags);
}

#include <twz/_sctx.h>
static uint32_t __conv_objperm_to_scp(uint64_t p)
{
	uint32_t perms = 0;
	if(p & OBJSPACE_READ) {
		perms |= SCP_READ;
	}
	if(p & OBJSPACE_WRITE) {
		perms |= SCP_WRITE;
	}
	if(p & OBJSPACE_EXEC_U) {
		perms |= SCP_EXEC;
	}
	return perms;
}

static uint64_t __conv_scp_to_objperm(uint32_t p)
{
	uint64_t perms = 0;
	if(p & SCP_READ) {
		perms |= OBJSPACE_READ;
	}
	if(p & SCP_WRITE) {
		perms |= OBJSPACE_WRITE;
	}
	if(p & SCP_EXEC) {
		perms |= OBJSPACE_EXEC_U;
	}
	return perms;
}

static bool __objspace_fault_calculate_perms(struct object *o,
  uint32_t flags,
  uintptr_t loaddr,
  uintptr_t vaddr,
  uintptr_t ip,
  uint64_t *perms)
{
	/* optimization: just check if default permissions are enough */
	uint32_t p_flags;
	if(!obj_get_pflags(o, &p_flags)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));

		return false;
	}
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
	bool ok = true;
	if(flags & OBJSPACE_FAULT_READ) {
		ok = ok && (dfl & MIP_DFL_READ);
	}
	if(flags & OBJSPACE_FAULT_WRITE) {
		ok = ok && (dfl & MIP_DFL_WRITE);
	}
	if(flags & OBJSPACE_FAULT_EXEC) {
		ok = ok && (dfl & MIP_DFL_EXEC);
	}
	if(dfl & MIP_DFL_READ)
		*perms |= OBJSPACE_READ;
	if(dfl & MIP_DFL_WRITE)
		*perms |= OBJSPACE_WRITE;
	if(dfl & MIP_DFL_EXEC)
		*perms |= OBJSPACE_EXEC_U;
	if(!ok) {
		*perms = 0;
		uint32_t res;
		if(secctx_fault_resolve(
		     (void *)ip, loaddr, (void *)vaddr, o, __conv_objperm_to_scp(flags), &res, true)
		   == -1) {
			return false;
		}
		*perms = __conv_scp_to_objperm(res);
	}

	bool w = (*perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		return false;
	}

	if(((*perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	return true;
}

struct object *obj_lookup_slot(uintptr_t oaddr, struct slot **slot)
{
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	*slot = slot_lookup(tl);
	if(!*slot) {
		return NULL;
	}
	struct object *obj = (*slot)->obj;
	if(obj) {
		krc_get(&obj->refs);
	}
	return obj;
}

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
	static size_t __c = 0;
	__c++;
	size_t idx = (loaddr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0 && !VADDR_IS_KERNEL(vaddr)) {
		struct fault_null_info info = twz_fault_build_null_info((void *)ip, (void *)vaddr);
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		return;
	}

	struct slot *slot;
	struct object *o = obj_lookup_slot(loaddr, &slot);

	if(o == NULL) {
		panic(
		  "no object mapped to slot during object fault: vaddr=%lx, oaddr=%lx, ip=%lx, slot=%ld",
		  vaddr,
		  loaddr,
		  ip,
		  loaddr / OBJ_MAXSIZE);
	}

	if(current_thread) {
		if(current_thread->_last_oaddr != loaddr || current_thread->_last_flags != flags) {
			current_thread->_last_oaddr = loaddr;
			current_thread->_last_flags = flags;
			current_thread->_last_count = 0;
		} else {
			current_thread->_last_count++;
			if(current_thread->_last_count > 50)
				panic("DOUBLE OADDR FAULT\n");
		}
	}
#if 0
	// uint64_t rsp;
	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	if(current_thread)
		printk("OSPACE FAULT %ld: ip=%lx loaddr=%lx (idx=%lx) vaddr=%lx flags=%x :: " IDFMT
		       " %lx\n",
		  current_thread ? current_thread->id : -1,
		  ip,
		  loaddr,
		  idx,
		  vaddr,
		  flags,
		  IDPR(o->id),
		  o->flags);
#endif

	uint64_t perms = 0;
	uint64_t existing_flags;

	bool do_map = !arch_object_getmap_slot_flags(NULL, slot, &existing_flags);
	do_map = do_map || (existing_flags & flags) != flags;

	// printk("A\n");

	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	if(do_map) {
		if(!VADDR_IS_KERNEL(vaddr) && !(o->flags & OF_KERNEL)) {
			if(!__objspace_fault_calculate_perms(o, flags, loaddr, vaddr, ip, &perms)) {
				goto done;
			}
			perms &= (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U);
		} else {
			perms = OBJSPACE_READ | OBJSPACE_WRITE;
		}
		if((flags & perms) != flags) {
			panic("TODO: this mapping will never work");
		}

		//	printk("B\n");
		spinlock_acquire_save(&slot->lock);
		if(!arch_object_getmap_slot_flags(NULL, slot, &existing_flags)) {
			//		if(o->flags & OF_KERNEL)
			//			arch_object_map_slot(NULL, o, slot, perms);
			//		else
			object_space_map_slot(NULL, slot, perms);
		} else if((existing_flags & flags) != flags) {
			arch_object_map_slot(NULL, o, slot, perms);
		}
		//	printk("C\n");
		spinlock_release_restore(&slot->lock);
	}

	if(o->flags & OF_ALLOC) {
		struct objpage p = { 0 };
		//	printk("X\n");
		p.page = page_alloc(PAGE_TYPE_VOLATILE,
		  (current_thread && current_thread->page_alloc) ? PAGE_CRITICAL : 0,
		  0); /* TODO: refcount, largepage */
		p.idx = (loaddr % OBJ_MAXSIZE) / mm_page_size(p.page->level);
		p.page->flags = PAGE_CACHE_WB;
		//	printk("Y\n");
		spinlock_acquire_save(&p.page->lock);
		arch_object_map_page(o, &p);
		spinlock_release_restore(&p.page->lock);
	} else {
		//	printk("P\n");
		struct objpage *p;
		enum obj_get_page_result gpr =
		  obj_get_page(o, loaddr % OBJ_MAXSIZE, &p, OBJ_GET_PAGE_ALLOC | OBJ_GET_PAGE_PAGEROK);
		//		  (o->flags & OF_PAGER) ? 0 : OBJ_GET_PAGE_ALLOC);
		switch(gpr) {
			case GETPAGE_OK:
				break;
			case GETPAGE_PAGER:
				goto done;
			case GETPAGE_NOENT: {
				panic("TODO: raise fault");
			} break;
		}
		assert(p && p->page);
		//	printk("P0\n");
		if(!(o->flags & OF_KERNEL))
			spinlock_acquire_save(&o->lock);
		//	printk("P1\n");
		if(!(o->flags & OF_KERNEL))
			spinlock_acquire_save(&p->page->lock);

#if 0
		printk(":: ofl=%lx, opfl=%lx, pcc=%d, %ld %ld\n",
		  o->flags,
		  p->flags,
		  p->page->cowcount,
		  o->refs.count,
		  o->mapcount.count);
#endif
		if(!(o->flags & OF_KERNEL)) {
			if(p->page->cowcount <= 1 && (p->flags & OBJPAGE_COW)) {
				p->flags &= ~(OBJPAGE_COW | OBJPAGE_MAPPED);
				p->page->cowcount = 1;
				//	printk("COW: reset %ld\n", p->idx);
			}

			if((p->flags & OBJPAGE_COW) && (flags & OBJSPACE_FAULT_WRITE)) {
				uint32_t old_count = atomic_fetch_sub(&p->page->cowcount, 1);
				//	printk("COW: copy %ld: %d\n", p->idx, old_count);

				if(old_count > 1) {
					struct page *np = page_alloc(p->page->type, 0, p->page->level);
					assert(np->level == p->page->level);
					np->cowcount = 1;
					void *cs = mm_ptov_try(p->page->addr);
					void *cd = mm_ptov_try(np->addr);

					bool src_fast = !!cs;
					bool dest_fast = !!cd;

					if(!cs) {
						cs = tmpmap_map_page(p->page);
					}
					if(!cd) {
						cd = tmpmap_map_page(np);
					}

					memcpy(cd, cs, mm_page_size(p->page->level));

					if(!dest_fast) {
						tmpmap_unmap_page(cd);
					}
					if(!src_fast) {
						tmpmap_unmap_page(cs);
					}

					assert(np->cowcount == 1);

					if(!(o->flags & OF_KERNEL))
						spinlock_release_restore(&p->page->lock);
					p->page = np;
					if(!(o->flags & OF_KERNEL))
						spinlock_acquire_save(&p->page->lock);
				} else {
					p->page->cowcount = 1;
				}

				p->flags &= ~OBJPAGE_COW;
				arch_object_map_page(o, p);
				p->flags |= OBJPAGE_MAPPED;
				// int x;
				/* TODO: better invalidation scheme */
				// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));

				// for(;;)
				//	;
			} else {
				// printk("P: %p %lx %lx\n", p->page, p->page->addr, p->flags);
				if(!(p->flags & OBJPAGE_MAPPED)) {
					arch_object_map_page(o, p);
					p->flags |= OBJPAGE_MAPPED;
					// int x;
					/* TODO: better invalidation scheme */
					// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
				}
			}
		} else {
			// bool m = arch_object_getmap_slot_flags(NULL, slot, &existing_flags);

			spinlock_acquire_save(&o->lock);
			if(!(p->flags & OBJPAGE_MAPPED)) {
				arch_object_map_page(o, p);
				p->flags |= OBJPAGE_MAPPED;
				// int x;
				/* TODO: better invalidation scheme */
				// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
			}
			spinlock_release_restore(&o->lock);
		}
		if(!(o->flags & OF_KERNEL))
			spinlock_release_restore(&p->page->lock);
		if(!(o->flags & OF_KERNEL))
			spinlock_release_restore(&o->lock);
	}
	// printk("Mapped successfully\n");
	//	do_map = false;
	//}
	/* TODO: put page? */

done:

	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	obj_put(o);
	slot_release(slot);
}
