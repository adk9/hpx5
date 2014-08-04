module load subversion
module list

make ARCH=edison-opt clean
rm gpu_setup.o
make ARCH=edison-opt

