#!/bin/bash -x

OP=$1
DIR=$2
shift

 ./opt/modules/Modules/3.2.10/init/bash
export hpx_cores=2
export hpx_thread=2
export PATH=/home/jayaajay/autotools/bin/:$PATH

case "$HPXMODE_AXIS" in
    smp)
     export NUMNODES=1
     CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 1\""
     ;;
    mpi)
     export NUMNODES=2
     CFGFLAGS+=" --with-mpi"
     CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --hostfile ${DIR}/scripts/hostfile\""
     ;;
    *)
     ;;
esac

function do_build() {
    echo "Building HPX in $DIR"
    cd "$DIR"
    
    echo "Bootstrapping HPX."
    git clean -xdf
    ./bootstrap
    
    if [ -d "./build" ]; then
      rm -rf ./build/
    fi
    mkdir build
    cd build

    if [ -d "./install" ]; then
      rm -rf ./install/
    fi
    mkdir install

    echo "Configuring HPX."
    eval "$CFG_CMD --prefix=${DIR}/build/install/ ${HPXDEBUG} ${CFGFLAGS} CFLAGS=\"-O3 -mfpu=vfpv3\" --enable-testsuite --enable-parallel-config"  
  
    echo "Building HPX."
    make -j 8
    make install

    if [ "$HPXMODE_AXIS" == "mpi" ]; then   
      cpush /home/jayaajay/jenkins /home/jayaajay/
    fi
}

case "$HPXDEBUG_CHOICE" in
  on)
    HPXDEBUG="--enable-debug"
    ;;
  off)
    HPXDEBUG="--disable-debug"
    ;;
esac

if [ "$OP" == "build" ]; then
   CFG_CMD="../configure"
   CFGFLAGS+=" CC=gcc"
   do_build
fi

if [ "$OP" == "run" ]; then
   echo "Running the regression test"
   cd "$DIR/build"
    
   make check -C tests
  
   # Check the output of the unit tests:
   if grep '^# FAIL: *0$' $DIR/build/tests/unit/test-suite.log
    then
      echo "FAIL: 0"
    else
      cat $DIR/build/tests/unit/test-suite.log
      exit 1
   fi

   if egrep -q "(ERROR:)\s+[1-9][0-9]*" $DIR/build/tests/unit/test-suite.log
     then
       cat $DIR/build/tests/unit/test-suite.log
       exit 1
   fi
fi

exit 0
