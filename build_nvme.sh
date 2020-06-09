#!/usr/bin/env bash
export PROJECT=x86_64
rm -r testing/objroot2
mkdir testing/objroot2
./us/gen_root_simple.sh projects/x86_64/build/us/sysroot/ testing/objroot2 | ./us/gen_root.py testing/objroot2 | ./us/append_ns_simple.sh testing/objroot2

ID=$(projects/x86_64/build/utils/objstat -i testing/objroot2/__ns)
./projects/x86_64/build/utils/mkimg testing/objroot2 -o nvme.img -n $ID
