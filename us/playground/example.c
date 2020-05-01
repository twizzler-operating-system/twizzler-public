/* this is an example program that creates an object, opens an object by path, and does some
 * manipulation */

/* normal C + POSIX headers work! */
#include <stdio.h>

/* access to the twizzler object API */
#include <twz/obj.h>

struct example_hdr {
	int x;
	void *some_data;
};

void example_create_object(void)
{
	/* a handle to an object. */
	twzobj obj;

	int r;
	/* create an object into handle obj, not copied from an existing object, without a public key,
	 * with default permissions READ and WRITE */
	if((r = twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		/* handle error */
	}

	/* get access to the base of the object */
	struct example_hdr *hdr = twz_object_base(&obj);
	/* hdr is now a v-ptr (virtual pointer) that can be dereferenced. It points to the base of the
	 * new object */

	hdr->x = 4;

	/* when we're done with the object, we can release it. */
	twz_object_release(&obj);

	/* Note that we do not have any way of gaining access to it
	 * again because:
	 *  1) we never named it,
	 *  2) we have forgotten the ID
	 */
}

void example_name_an_object(twzobj *obj)
{
	/* Warning - this API is unstable */

	/* let's say we have an object in handle obj, and we want to give it a name so we can access it
	 * later. We can use the naming API for that: */
	twz_name_assign(twz_object_guid(obj), "/some/path");
	/* note: this can fail */
}

void example_open_an_object_by_name(const char *name)
{
	twzobj obj;

	/* open an object named 'name' for reading and writing */
	twz_object_init_name(&obj, name, FE_READ | FE_WRITE);
	/* note: this can fail */

	struct example_hdr *hdr = twz_object_base(&obj);
	printf("x is %d\n", hdr->x);
}

void example_load_a_pointer(twzobj *obj)
{
	/* say we have an object obj, and it has this 'example hdr' at its base */
	struct example_hdr *hdr = twz_object_base(obj);

	/* now this struct has a pointer inside it, 'some_data'. We want to load it! But this is a
	 * 'p-ptr', a persistent pointer. We have to first translate it into a v-ptr like so: */
	struct foo *some_data_v = twz_object_lea(obj, hdr->some_data);
	/* and now we can do stuff with this. Note: this call either succeeds or throws */
}

void example_store_a_pointer(twzobj *obj, void *some_virt_addr)
{
	struct example_hdr *hdr = twz_object_base(obj);

	/* storing a pointer is a little more complicated. We need to know what object the pointer is
	 * referring to. We have a couple options: */

	/* 1) let's say we just have a virtual address we want to store. We can store it into the object
	 * as: */
	hdr->some_data = twz_ptr_swizzle(obj, some_virt_addr, FE_READ | FE_WRITE);
	/* this call will either succeed or throw */

	/* we can have a little more control like so: */

	twz_ptr_store_guid(obj,
	  &hdr->some_data /* location to store the p-ptr */,
	  NULL /* target object. if NULL, treat next argument as a v-ptr. If not NULL, treat next
	          argument as a p-ptr specifying an offset within this target object */
	  ,
	  some_virt_addr /* the pointer we are storing */,
	  FE_READ | FE_WRITE);

	/* there is also a variant of this called twz_ptr_store_name that creates a p-ptr with a name
	 * and on offset */
}

void example_create_temporary_object(void)
{
	twzobj obj;
	/* lets create a volatile object */
	twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE);

	/* by default, this object is tied to our thread, that is, the kernel will not delete it until
	 * at least our thread object is deleted. */

	/* let's delete it */
	twz_object_delete(&obj, 0);

	/* we can still use this object, since it's tied to our thread. If we were to untie it, it would
	 * be deleted soon afterwards. It will also be deleted on reboot. */

	/* a common pattern is to tie a bunch of objects to some anchor while creating data structures
	 * and then untie them. That makes cleanup automatic until the objects are done being created.
	 * */
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	printf("Hello, World!\n");
	return 0;
}
