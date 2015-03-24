qsub -I -l walltime=00:45:00 -l nodes=2:ppn=32

aprun -n 1 -d 32 -b ./bfs --hpx-threads=32 --hpx-transport=mpi

aprun -n 1 -d 32 -b sssp -m bfs --hpx-threads=1 --hpx-transport=mpi ./input/Random4-C.9.0.gr ./input/Random4-C.9.0.ss


delta
====
aprun -n 1 -d 32 -b sssp -c -z 40000 --hpx-threads=32 --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-transport=smp ./input/Random4-C.9.0.gr ./input/Random4-C.9.0.ss 

bfs
===
 aprun -n 1 -d 32 -b sssp -m bfs --hpx-threads=32 --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-transport=smp ./input/Random4-C.9.0.gr ./input/Random4-C.9.0.ss
