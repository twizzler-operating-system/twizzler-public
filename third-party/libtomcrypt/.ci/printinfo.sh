#!/bin/bash

version=$(git describe --tags --always --dirty 2>/dev/null)
if [ ! -e ".git" ] || [ -z $version ]
then
   version=$(grep "^VERSION=" makefile_include.mk | sed "s/.*=//")
fi
echo "Testing version:" $version
#grep "VERSION=" makefile | perl -e "@a = split('=', <>); print @a[1];"`

# get uname
echo "uname="`uname -a`

# get gcc name
if [ -z ${CC} ]
then
   CC="gcc"
fi
echo "${CC}="`${CC} -dumpversion`
echo

# ref:         HEAD -> develop
# git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046
# commit time: 2019-06-11 07:55:21 +0200
