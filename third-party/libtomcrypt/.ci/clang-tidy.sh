#!/bin/bash

# output version
bash .ci/printinfo.sh

# tested with clang-tidy from llvm-6.0.0
# not tested with Travis-CI

#### we use the main test sets:
# readability
# misc
# clang-analyzer
# google
# performance
# modernize
# cert
# bugprone
# portability

#### the following checks are skipped
# google-readability-function-size
# readability-function-size
# google-readability-casting
# readability-braces-around-statements
# misc-macro-parentheses
# clang-analyzer-valist.Uninitialized

echo "Run clang-tidy version"

clang-tidy --version || exit 1

echo "Run clang-tidy..."

clang-tidy src/*/*.c src/*/*/*.c src/*/*/*/*.c src/*/*/*/*/*.c -warnings-as-errors='*' --quiet --checks=-*,\
readability-*,-readability-function-size,-readability-braces-around-statements,\
misc-*,-misc-macro-parentheses,\
clang-analyzer-*,-clang-analyzer-valist.Uninitialized,\
google-*,-google-readability-function-size,-google-readability-casting,\
performance-*,\
modernize-*,\
cert-*,\
bugprone-*,\
portability-* -- -DUSE_LTM -DLTM_DESC -Isrc/headers -I../libtommath || { echo "clang-tidy FAILED!"; exit 1; }

echo "clang-tidy ok"

exit 0

# ref:         HEAD -> develop
# git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046
# commit time: 2019-06-11 07:55:21 +0200
