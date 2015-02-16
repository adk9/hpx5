#!/bin/bash

DIR=$1
shift

set -xe
cd $DIR

# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep "FAIL:" tests/unit/test-suite.log
then
    cat tests/unit/test-suite.log
    exit 1
fi

exit 0
