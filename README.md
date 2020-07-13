# README #

Kernel and userspace for Twizzler. Requires a toolchain to build (see Building). Twizzler is a
research OS, focused on designing a programming model and kernel support for NVM. We're not quite
production ready, but we're trying to get there! :)

See https://twizzler.io/about.html for an overview of our goals.

Building
--------

See doc/building.txt for instructions. Note that the build system is rather complex, and thus may
break on systems that we haven't tested it on. Currently you must be on Linux to build Twizzler.


Writing some test code
----------------------

See us/playground/README.md. For an example of some of the Twizzler API, see us/playground/example.c

For an example driver, see doc/device_example.

Documentation
-------------

Docs can be build with the following command:
  
  make doc.pdf PROJECT=x86_64

Note that the documentation does lag a bit behind the actual APIs.

