#!/bin/bash

# output version
bash .ci/printinfo.sh

if [ -f /proc/cpuinfo ]
then
   MAKE_JOBS=$(( ($(cat /proc/cpuinfo | grep -E '^processor[[:space:]]*:' | tail -n -1 | cut -d':' -f2) + 1) * 2 + 1 ))
else
   MAKE_JOBS=8
fi

echo "$1 (Build Only, $2, $3)..."
make clean 1>/dev/null 2>/dev/null
echo -n "building..."
touch testok.txt
make -j$MAKE_JOBS -f $3 test tv_gen CFLAGS="$2 $CFLAGS $4" EXTRALIBS="$5" 1>gcc_1.txt 2>gcc_2.txt || (echo "build $1 failed see gcc_2.txt for more information" && cat gcc_2.txt && rm -f testok.txt && exit 1)
if find testok.txt -type f 1>/dev/null 2>/dev/null ; then
   echo "successful"
   exit 0
fi
exit 1

# ref:         HEAD -> develop
# git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046
# commit time: 2019-06-11 07:55:21 +0200
