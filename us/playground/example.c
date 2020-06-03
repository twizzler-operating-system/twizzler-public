/* this is an example program that creates an object, opens an object by path, and does some
 * manipulation */

/* normal C + POSIX headers work! */
#include <stdio.h>

/* access to the twizzler object API */
#include <twz/name.h>
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
	if(twz_object_init_name(&obj, name, FE_READ | FE_WRITE) < 0)
		return;
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
	(void)some_data_v;
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
	if(twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE) < 0)
		return;

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

void example_allocate_from_object(void)
{
	/* it's often the case that we'll want to allocate some amount of memory from within an object.
	 * Unix-file-like objects and other already structured objects (like PTYs) maybe not, but with
	 * data structures, definitely.
	 *
	 * Twizzler provides an API for allocating data within objects in two ways:
	 * 1) The twz/alloc.h header, which allows the programmer to embed a 'twzoa_header' struct
	 *    within their object whereever they like, and initialize that to allocate from within some
	 *    range of the object.
	 * 2) (AND PROBABLY EASIEST), Twizzler provides default allocation functions for objects _as
	 *    long as_ that object was initialized to support them. This example will focus on this,
	 *    because it's probably the easiest to do. */

	twzobj obj;
	if(twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE) < 0)
		return;

	/* init the object to respond to the default allocation API */
	twz_object_build_alloc(&obj, 0);

	/* that second argument is an offset within the object starting from the base (as returned by
	 * twz_object_base). This is to enable the user to have some data at the start of the object
	 * (like some common header) */

	/* now we can allocate from the object. Here's some size-8 data. */
	void *p = twz_object_alloc(&obj, 8);

	/* note that the returned pointer is a persistent pointer! So it must be LEA'd to be used: */
	void *v = twz_object_lea(&obj, p);
	(void)v;

	/* to free, you can pass either the p-ptr or the v-ptr to free. This is because Twizzler will
	 * assume you're referring to some data within the object you pass to it, so don't screw that
	 * up! :) */
	twz_object_free(&obj, p); // or v
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	printf("Hello, World!\n");
	return 0;
}
