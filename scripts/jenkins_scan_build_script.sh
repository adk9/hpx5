#!/bin/bash -x

OP=$1
DIR=$2
shift

. /opt/modules/Modules/3.2.10/init/bash
module load llvm/3.6.0

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
CFG_CMD="../configure"
eval "/u/crest-team/LLVM/llvm-3.6.0.src/tools/clang/tools/scan-build/scan-build --use-analyzer=/usr/bin/clang $CFG_CMD CC=clang CFLAGS=\"-std=gnu99 -O0\" --enable-debug --enable-instrumentation --enable-logging"  

echo "Building HPX."
eval "/u/crest-team/LLVM/llvm-3.6.0.src/tools/clang/tools/scan-build/scan-build --use-analyzer=/usr/bin/clang -v -v -o  $DIR/clangScanBuildReports make" 

exit 0
