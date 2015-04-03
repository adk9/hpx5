#!/bin/bash -x

OP=$1
DIR=$2
shift

function add_init() {
case "$SYSTEM" in
  CREST_cutter)
    . /opt/modules/Modules/3.2.10/init/bash
    module load llvm/3.6.0
    ;;
  *)
    echo "Unknown system $SYSTEM."
    exit 1
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
    
    echo "Configuring HPX."
    eval "/u/crest-team/LLVM/llvm-3.6.0.src/tools/clang/tools/scan-build/scan-build --use-analyzer=/usr/bin/clang $CFG_CMD CC=clang CFLAGS=\"-std=gnu99 -O0\" --enable-debug --enable-instrumentation --enable-logging"  
  
    echo "Building HPX."
    eval "/u/crest-team/LLVM/llvm-3.6.0.src/tools/clang/tools/scan-build/scan-build --use-analyzer=/usr/bin/clang -v -v -o  $DIR/clangScanBuildReports make" 
}

add_init

if [ "$OP" == "build" ]; then
    case "$SYSTEM" in
      CREST_cutter)
        CFG_CMD="../configure"
        ;;
      *)
       exit 1
       ;;
   esac
   do_build
fi

exit 0
