# Pre-Built Image README

This zip file contains the following files: 

  * boot.iso: An ISO image containing the kernel file, initrd image, and GRUB bootloader to start
	Twizzler.
  * nvme.img: An image used for the Twizzler NVMe driver that contains a lot of the Twizzler
	userspace.
  * start.sh: A shell script that creates a pmem.img file and starts QEMU with the necessary flags
	for Twizzler to run.

The start.sh script can be run as,

    ./start.sh

which will create a new pmem.img (an image file used as an NVDIMM in QEMU), replacing the old one.
If you want your writes to persistent objects to remain over reboots, you can specify

    NV=keep ./start.sh

Additional flags to QEMU can be passed by setting the QEMU_FLAGS env variable.

NOTE: Twizzler makes use of VT-x, which means that KVM (the Linux kernel virtual machine driver)
will need to be in nested mode. In recent versions, this is on by default. But you might need to
enable it if Twizzler complains on bootup of missing virtualization extensions.

Booting and Basics
------------------
When Twizzler boots you should see a login prompt. Just hit enter, and you'll be logged in. Note
that in this image, a lot of security features have been disabled for ease-of-use (I told you it's
not production ready!). We're still finishing up some security APIs, and we'd rather not release
them until they are stable.

Once you've logged in, you'll see a bash prompt. Many commands are available. Note that we provide
this environment for familiarity. However, our unix environment is a work-in-progress (and is, in
fact, not the primary goal of our work), thus some features and functionality don't work. But try
out ls, the most important command! :)

The persistent memory emulated by QEMU will be available for the Twizzler API when creating
persistent objects (using twz_object_new, for example). But it's also available through the unix
environment, with a namespace "mounted" at /nvr1.0. Files written in that directory will persist
across reboots.

Demos
-----
We recently ported vim (NOTE: some functionality doesn't work, but you can create and edit files!),
binutils (ld, as, etc), and gcc. So, you can try writing a normal "hello world" program in C,
compiling it, and running it!


