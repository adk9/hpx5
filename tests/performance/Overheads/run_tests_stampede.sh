#!/bin/bash
#SBATCH -J tests16         # job name
#SBATCH -o tests.o%j       # output and error file name (%j expands to jobID)
#SBATCH -n 2               # total number of mpi tasks requested
#SBATCH -N 2               # number of nodes
#SBATCH -p development     # queue (partition) -- normal, development, etc.
#SBATCH -t 01:30:00        # run time (hh:mm:ss) - 1.5 hours
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/perfmain
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_gas_alloc
ibrun $HOME/build/tests/performance/Overheads/time_gas_addr_trans
ibrun -np 2 $HOME/build/tests/performance/Overheads/time_lco_chan
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_sema
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_future
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allgather 10
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allgather 100
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allgather 1000
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allreduce 10
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allreduce 100
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_allreduce 1000
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_alltoall 10
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_alltoall 100
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_alltoall 1000
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_netfutures
ibrun -np $SLURM_NNODES $HOME/build/tests/performance/Overheads/time_lco_netfutures_msgSize -c 1

