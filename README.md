# README #

Twizzler is a research operating system designed to explore novel programming models for new memory
hierarchy designs. In particular, we are focused on the opportunities presented by byte-addressable
non-volatile memory technologies. Twizzler provides a data-centric programming environment that
allows programmers to operate on persistent data like it's memory -- because it is!

This repo contains source code for the kernel and userspace, along with a build system that
bootstraps a Twizzler userspace (including porting existing POSIX software). You can write code for
it and play around! We're not quite production ready, but we're trying to get there! :)

See https://twizzler.io/about.html for an overview of our goals.

Building
--------

See doc/building.txt for instructions. Note that the build system is rather complex, and thus may
break on systems that we haven't tested it on. Currently you must be on Linux to build Twizzler.

You can download pre-built images at https://twizzler.io/download.html.

Writing some test code
----------------------

See us/playground/README.md. For an example of some of the Twizzler API, see us/playground/example.c

For an example driver, see doc/device_example.

Documentation
-------------

Docs can be build with the following command:
  
  make doc.pdf PROJECT=x86_64

Note that the documentation does lag a bit behind the actual APIs.

