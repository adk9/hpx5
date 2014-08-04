#PBS -q regular
#PBS -l mppwidth=6144
#PBS -l walltime=30:00
#PBS -N my_job
#PBS -j oe 
##PBS -V

cd $PBS_O_WORKDIR
setenv OMP_NUM_THREADS 12

# for Intel compiled programs
# the "-cc numa_node" option should be used if the number of threads is less than or equal 12
aprun -n 512 -N 2 -S 1 -d 12 -cc numa_node ./bench_gtc_edison_opt ./input/A.txt 100 64
