#!/bin/bash

# output version
bash .ci/printinfo.sh

bash .ci/build.sh " $1" "$2 -O2" "$3 IGNORE_SPEED=1" "$4" "$5"
if [ -a testok.txt ] && [ -f testok.txt ]; then
   echo
else
   echo
   echo "Test failed"
   exit 1
fi

rm -f testok.txt
bash .ci/build.sh " $1" "$2 -Os" "$3 IGNORE_SPEED=1 LTC_SMALL=1" "$4" "$5"
if [ -a testok.txt ] && [ -f testok.txt ]; then
   echo
else
   echo
   echo "Test failed"
   exit 1
fi

rm -f testok.txt
bash .ci/build.sh " $1" "$2" "$3 LTC_DEBUG=1" "$4" "$5"
if [ -a testok.txt ] && [ -f testok.txt ]; then
   echo
else
   echo
   echo "Test failed"
   exit 1
fi

rm -f testok.txt
bash .ci/build.sh " $1" "$2" "$3" "$4" "$5"
if [ -a testok.txt ] && [ -f testok.txt ]; then
   echo
else
   echo
   echo "Test failed"
   exit 1
fi

exit 0

# ref:         HEAD -> develop
# git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046
# commit time: 2019-06-11 07:55:21 +0200
