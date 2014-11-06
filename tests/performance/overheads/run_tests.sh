mpirun perfmain
mpirun time_gas_alloc
mpirun time_gas_addr_trans
mpirun -np 2 time_lco_chan
mpirun time_lco_sema
mpirun time_lco_and
mpirun time_lco_future
mpirun time_lco_allgather 10
mpirun time_lco_allgather 100
mpirun time_lco_allgather 1000
mpirun time_lco_allgather 10000
mpirun time_lco_allreduce 10
mpirun time_lco_allreduce 100
mpirun time_lco_allreduce 1000
mpirun time_lco_allreduce 10000
mpirun time_lco_alltoall 10
mpirun time_lco_alltoall 100
mpirun time_lco_alltoall 1000
mpirun time_lco_netfutures
mpirun time_lco_netfutures_msgSize -c 1
