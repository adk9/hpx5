#! /bin/sh
#PBS -N make_check
#PBS -l walltime=0:05:00
#PBS -l nodes=2:ppn=1
#PBS -e make_check.$PBS_JOBID.err
#PBS -o make_check.$PBS_JOBID.out
export CC=mpicc
export LIBRARY_PATH=/home/benjmart/extern/check/lib/:/home/benjmart/photon/libphoton/lib/
export LD_LIBRARY_PATH=/home/benjmart/extern/check/lib/:/home/benjmart/photon/libphoton/lib/:$LD_LIBRARY_PATH
export C_INCLUDE_PATH=/home/benjmart/extern/check/include:/home/benjmart/photon/libphoton/include/
cd /home/benjmart/hpx/hpx5/branches/benjmart_photon
make check
#mpirun hpx/hpx4/trunk/tests/hpxtest

