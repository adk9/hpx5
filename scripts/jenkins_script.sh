#!/bin/bash -x



OP=$1
DIR=$2
shift

if [ "$HPXMODE_AXIS" == smp ] ;
then
  export NUMNODES=1
elif [ "$SYSTEM" == CREST_cutter ] ;
then
  export NUMNODES=10
else
  export NUMNODES=16
fi

function add_init() {
case "$SYSTEM" in
  CREST_cutter)
    . /opt/modules/Modules/3.2.10/init/bash
    export INPUT_DIR=/u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n
    export RUNCMD="mpirun -n $NUMNODES --map-by node:PE=16 --tag-output"
    module load openmpi/1.8.4_thread 
    ;;
  HPX5_BIGRED2)
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/N/home/h/p/hpx5/BigRed2/new_modules/bin:$PATH
    export INPUT_DIR=/N/dc2/scratch/zalewski/dimacs/Random4-n
    export RUNCMD="aprun -n $NUMNODES -N 1 -b -d $QSUB_NUMCORES"
    ;;
  MARCINS_SWAN)
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export PATH=/home/users/p02087/tools/bin:$PATH
    export CRAYPE_LINK_TYPE=dynamic
    export INPUT_DIR=/lus/scratch/p02087/crest/Random4-n
    export RUNCMD="aprun -n $NUMNODES -N 1 -b -d $QSUB_NUMCORES"
    ;;
  *)
    echo "Unknown system $SYSTEM."
    exit 1
    ;;
esac
case "$SIZE_CHOICE" in
  tiny)
    export LARGE_SIZE=10
    export SMALL_SIZE=10
    ;;
  medium)
    export LARGE_SIZE=20
    export SMALL_SIZE=12
    ;;
  large)
    export LARGE_SIZE=22
    export SMALL_SIZE=14
    ;;
esac
case "$HPXCORES_AXIS" in
  all)
    export HPXCORES=" --hpx-threads=$QSUB_NUMCORES"
    ;;
  *)
    export HPXCORES=" --hpx-threads=$HPXCORES_AXIS"
    ;;
esac
case "$BUILD_AXIS" in
  static)
    CFGFLAGS+=" --enable-static --disable-shared"
    ;;
  *)
    CFGFLAGS+=" --disable-static --enable-shared"
    ;;
esac
}

function add_mpi() {
case "$SYSTEM" in
  CREST_cutter)
    module load openmpi/1.8.4_thread
    CFGFLAGS+=" --with-mpi=ompi"
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    CFGFLAGS+=" --with-mpi"
    ;;
esac
}

function add_photon() {
CFGFLAGS+=" --enable-photon"
case "$SYSTEM" in
  CREST_cutter)
    export HPX_PHOTON_IBDEV=$HPXDEV_AXIS
    export HPX_PHOTON_BACKEND=verbs
    # verbs/rdmacm library not in jenkins node config
    export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/usr/lib64:$LIBRARY_PATH
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
    CFGFLAGS+=" --with-pmi --with-hugetlbfs"
    ;;
esac
}

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
    $CFG_CMD --prefix="${DIR}/build/install/" ${HPXDEBUG} ${CFGFLAGS} CFLAGS="-O3 -g"
    
    echo "Building HPX."
    make -j 128
    make install
}

CFGFLAGS=" ${JEMALLOC_AXIS} --enable-apps --enable-parallel-config"

add_init

case "$HPXMODE_AXIS" in
    photon)
	add_mpi
        add_photon
	;;
    mpi)
	add_mpi	
	;;
    *)
	;;
esac

case "$HPXDEV_AXIS" in
    qib0)
        export PSM_MEMORY=large
	RUNCMD+=" --mca btl_openib_if_include ${HPXDEV_AXIS}"
	;;
    mlx4_0)
	RUNCMD+=" --mca mtl ^psm --mca btl_openib_if_include ${HPXDEV_AXIS}"
	;;
    *)
	;;
esac

case "$SYSTEM" in
  CREST_cutter)
    CFGFLAGS+=" CC=gcc"
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    CFGFLAGS+=" CC=cc"
    ;;
  *)
    exit 1
    ;;
esac

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
    do_build
fi

if [ "$OP" == "run" ]; then
    cd "$DIR/build"

    # TBD: fine tune the limits
    # TBD: copy the inputs to the jenkins accounts
        cd apps/libPXGL/examples
	# Trying to get NFS to find the files on cutter
	if [ "$SYSTEM" == "CREST_cutter" ]; then
          $RUNCMD find `pwd` > /dev/null
	  $RUNCMD ls `pwd`/.libs/lt-sssp
          sleep 5
        fi
        # Delta-Stepping
	$RUNCMD `pwd`/sssp -q $TIME_CHOICE -c -z 40000 $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.$LARGE_SIZE.0.gr $INPUT_DIR/Random4-n.$LARGE_SIZE.0.ss || { echo 'SSSP test failed' ; exit 1; }
        # Chaotic
        $RUNCMD `pwd`/sssp -q $TIME_CHOICE -c $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.$SMALL_SIZE.0.gr $INPUT_DIR/Random4-n.$SMALL_SIZE.0.ss || { echo 'SSSP test failed' ; exit 1; }
        # Distributed control
        $RUNCMD `pwd`/sssp -q $TIME_CHOICE -d $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.$SMALL_SIZE.0.gr $INPUT_DIR/Random4-n.$SMALL_SIZE.0.ss || { echo 'SSSP test failed' ; exit 1; }
fi

exit 0
