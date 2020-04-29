Creating a program to play around with Twizzler
-----------------------------------------------

1) create a C file in us/playground, eg. us/playground/foo.c

2) add this to the PLAYGROUND\_PROGS variable in us/playground/include.mk:

	PLAYGROUND\_PROGS=example foo

3) Start writing code in foo.c; this file will be automatically compiled into a program called 'foo'
and placed in usr/bin in the sysroot.

