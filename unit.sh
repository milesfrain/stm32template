#!/bin/bash

# Run all unit tests

# Check unit tests and code coverage
unit_ret=0
for testdir in $(cat test-dirs.txt)
do
    echo $testdir
    pushd $testdir
    make lcov
    unit_ret=$((unit_ret || $?))
    popd
done

echo exit status $unit_ret

exit $unit_ret
