# README #

Kernel for Twizzler. Requires a toolchain to build. Currently setting up core kernel components.

Building
--------

First you'll need to install a toolchain. Run,

	PROJECT=<project> make tools-prep

Replace 'project' with a valid project (see the projects/ directory; e.g. PROJECT=x86_64 make ...) and follow the on-screen instructions. This will take a while.

Next, you're ready to build! Run

	PROJECT=<project> make all userspace

to build the kernel and the userspace libraries and applications.

Finally, you can run make test to test the system via qemu. A suggested testing command:
	
	PROJECT=x86_64 QEMU_FLAGS='-smp 4' make test

will give Twizzler 4 CPUs to play with.

