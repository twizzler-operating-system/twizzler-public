Twizzler Overview
=================

The Twizzler Operating System is designed for non-volatile memory and to unify the hardware
programming model with the software programming model. This means that it is, fundamentally, a
single-level store, exokernel-like system that:

  * Places most of the system in userspace.
  * Has _very_ limited in-kernel blocking.
  * Allows hardware to act autonomously (if capable).
  * Provides persistent object support as the primary data abstraction.

Twizzler does away with a number of traditional abstractions that a Unix has (note we provide enough
Unix emulation to have a useable C runtime and standard library, and many POSIX functions do work):

  * Files are not a thing in Twizzler. Instead, Twizzler is based around _objects_. Threads can
	access objects through load and store instructions (they are memory objects). Persistence is
	automatic and configurable.
  * There is no notion of a process. Instead, Twizzler provides the core components of a process as
	separate, individually controllable abstractions. These include _views_ (an abstraction for
	virtual-memory machines), _security contexts_ (which control access for hardware and software
	to objects, and restrict control-flow transfers), and _threads_ (which are similar to threads in
	standard OSes).
  * File systems are not needed on Twizzler. Objects are accessed by an ID (no file handles, no
	inodes). When names are needed, name resolvers can be used, but the naming system is
	disconnected from data storage.
  * Traditional Unix users, groups, etc., are not used in Twizzler. Instead, we provide a more
	robust and fine-grained security mechanism based on cryptographically signed capabilities,
	security contexts, and content-derived names.

Twizzler programs must be _statically linked_ at the moment. This limitation will soon be removed. A
Twizzler program typically has the following core components:

  * The program itself, the executable.
  * The program is linked to musl, a C library, to provide a C standard library.
  * The program is also linked to Twix (libtwix), a library that emulates the Linux system call
	interface (needed for musl to be happy).
  * Finally, we link to libtwz, the Twizzler standard library, which provides a runtime for Twizzler
	(eg. fault handling, userspace-level object management, etc).

Twizzler's primary abstractions, of which there are few, build the base of the system. Further
structure can be imposed on objects in userspace, but the core system provides only the following:

  * Basic object manipulation. Objects are flat memory spaces of data, with a meta-data page at the
	end. The kernel understands some of the meta-data page, as it is used for ID derivation and
	security purposes. Objects can be created, deleted, and configured. See doc/us/objects.md.
  * Pointer manipulation. Loading, resolving, storing, etc. See doc/us/pointers.md.
  * Kernel State Objects (KSOs). A KSO is an object whose format has a specified internal layout
	that the kernel understands. These are used to configure the running kernel, or reload a
	previous running state. These include:
	  * Views 
	  * Security Contexts
	  * Root KSOs
	  * Threads
	  * Devices

	See doc/core/kso.md for details.
  * Thread control. Threads can be created, waited on, can exit, can signal other threads, and can
	synchronize. See doc/us/threads.md.
  * Structured Objects. Twizzler provides a mechanism for generically adding structure that can be
	discovered inside an object. See us/structured_objects.md. The core structured objects Twizzler
	provides are:
	  * bstream (a simplex, byte-oriented stream).
	  * dgram (a duplex data-gram interface).
	  * event (a system for waiting on events, similar to select/poll/epoll).
	  * bstreamd (a duplex, byte-oriented stream).
	  * user (a user file, describing a user).
	  * kring (a key-ring object).
	  * ku (a public key object).
	  * kr (a private key object).

	See the associated doc/us/_type_.md file for details on these.

