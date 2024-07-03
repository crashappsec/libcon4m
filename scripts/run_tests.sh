#!/bin/bash
set -e

rm -rf ./debug ./build ./release

comp='clang-18'
# comp='gcc'

${comp} -v

# kind='debug'
kind='build'

CC="${comp}" ./dev "${kind}"
./"${kind}"/c4test ./tests