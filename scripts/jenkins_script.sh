#!/bin/bash -x



OP=$1
DIR=$2
shift

function add_init() {
case "$SYSTEM" in
  CREST_cutter)
    . /opt/modules/Modules/3.2.10/init/bash
    export INPUT_DIR=/u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n
    export RUNCMD="mpirun -n 10 --map-by node:PE=16 --tag-output"
    ;;
  HPX5_BIGRED2)
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/N/home/h/p/hpx5/BigRed2/new_modules/bin:$PATH
    export INPUT_DIR=/N/dc2/scratch/zalewski/dimacs/Random4-n
    export RUNCMD="aprun -n 10 -N 1 -d 16"
    ;;
  *)
    echo "Unknown system $SYSTEM."
    exit 1
    ;;
esac
case "$SIZE_CHOICE" in
  long)
    export TIMEOUT="60"
    ;;
  *)
    ;;
esac
case "$HPXCORES_AXIS" in
  all)
    ;;
  *)
    export HPXCORES=" --hpx-cores=$HPXCORES_AXIS"
esac
}

function add_mpi() {
case "$SYSTEM" in
  CREST_cutter)
    module load openmpi/1.8.4_thread
    CFGFLAGS+=" --with-mpi=ompi"
    ;;
  HPX_BIGRED2)
    CFGFLAGS+=" --with-mpi"
    ;;
esac
}

function add_photon() {
CFGFLAGS+=" --enable-photon"
case "$SYSTEM" in
  CREST_cutter)
    export HPX_PHOTON_IBDEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=verbs
    # verbs/rdmacm library not in jenkins node config
    export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/usr/lib64:$LIBRARY_PATH
  HPX_BIGRED2)
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
    CFGFLAGS+=" --with-pmi --with-hugetlbfs"
esac
}

function do_build() {
    echo "Building HPX in $DIR"
    cd "$DIR"
    
    echo "Bootstrapping HPX."
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
    eval $CFG_CMD
    
    echo "Building HPX."
    make -j 128
    make install
}

CFGFLAGS=" ${JEMALLOC_AXIS} ${BUILD_AXIS} --enable-apps --enable-parallel-config"

add_init

case "$HPXMODE" in
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

case "$HPXIBDEV" in
    qib0)
        export PSM_MEMORY=large
	RUNCMD+=" --mca btl_openib_if_include ${HPXIBDEV}"
	;;
    mlx4_0)
	RUNCMD+=" --mca mtl ^psm --mca btl_openib_if_include ${HPXIBDEV}"
	;;
    *)
	;;
esac

case "$SYSTEM" in
  CREST_cutter)
    CFGFLAGS+=" CC=gcc"
    ;;
  HPX_BIGRED2)
    CFGFLAGS+=" CC=cc"
    ;;
  *)
    exit 1
    ;;
esac

if [ "$OP" == "build" ]; then
    CFG_CMD="../configure --prefix=${DIR}/build/install/ ${HPXDEBUG} ${CFGFLAGS}"
    do_build
fi

if [ "$OP" == "run" ]; then
    cd "$DIR/build"

    # TBD: fine tune the limits
    # TBD: copy the inputs to the jenkins accounts
        cd "$DIR/apps/libPXGL/examples"
        # Delta-Stepping
	$(RUNCMD) sssp -q $TIMEOUT -c -z 40000 $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.22.0.gr $INPUT_DIR/Random4-n.22.0.ss
        # Chaotic
        $(RUNCMD) sssp -q $TIMEOUT $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.20.0.gr $INPUT_DIR/Random4-n.20.0.ss 
        # Distributed control
        $(RUNCMD) sssp -q $TIMEOUT -d $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.20.0.gr $INPUT_DIR/Random4-n.20.0.ss
fi

exit 0
