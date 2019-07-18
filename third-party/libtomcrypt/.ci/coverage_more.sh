#!/bin/bash

set -e

if [ "$#" = "1" -a "$(echo $1 | grep 'gmp')" != "" ]; then
   ./test t gmp
fi

./sizes
./constants

for i in $(for j in $(echo $(./hashsum -h | awk '/Algorithms/,EOF' | tail -n +2)); do echo $j; done | sort); do echo -n "$i: " && ./hashsum -a $i tests/test.key ; done > hashsum_tv.txt
difftroubles=$(diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt | grep '^<') || true
if [ -n "$difftroubles" ]; then
  echo "FAILURE: hashsum_tv.tx"
  diff -i -w -B hashsum_tv.txt notes/hashsum_tv.txt
  echo "hashsum failed"
  exit 1
else
  echo "hashsum okay"
fi


exit 0

# ref:         HEAD -> develop
# git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046
# commit time: 2019-06-11 07:55:21 +0200
