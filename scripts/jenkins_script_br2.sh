#!/bin/bash

module load java
module unload PrgEnv-cray
module load PrgEnv-gnu
module load craype-hugepages8M
export CRAYPE_LINK_TYPE=dynamic

export PATH=/N/u/hpx5/BigRed2/modules/help2man/bin:$PATH
export HELP2MAN=/N/u/hpx5/BigRed2/modules/help2man/bin/help2man
export PATH=/N/u/hpx5/BigRed2/modules/texinfo/bin:$PATH
export PATH=/N/u/hpx5/BigRed2/modules/M4/bin:$PATH
export PATH=/N/u/hpx5/BigRed2/modules/autoconf/bin:$PATH
export PATH=/N/u/hpx5/BigRed2/modules/automake/bin:$PATH
export PATH=/N/u/hpx5/BigRed2/modules/libtool/bin:$PATH

export C_INCLUDE_PATH=/N/u/hpx5/BigRed2/modules/libtool/include:$C_INCLUDE_PATH
export LD_LIBRARY_PATH=/N/u/hpx5/BigRed2/modules/libtool/lib:$LD_LIBRARY_PATH

DIR=$1
shift

set -xe
cd $DIR

cd build
# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep "FAIL:" tests/unit/test-suite.log
then
    cat tests/unit/test-suite.log
    exit 1
fi

exit 0
