ibrun perfmain
ibrun -np 1 time_gas_alloc
ibrun -np 1 time_gas_addr_trans
ibrun -np 2 time_lco_chan
ibrun -np 1 time_lco_sema
ibrun -np 1 time_lco_and
ibrun -np 1 time_lco_future
ibrun -np 1 time_lco_allgather 10
ibrun -np 1 time_lco_allgather 100
ibrun -np 1 time_lco_allgather 1000
ibrun -np 1 time_lco_allgather 10000
ibrun -np 1 time_lco_allreduce 10
ibrun -np 1 time_lco_allreduce 100
ibrun -np 1 time_lco_allreduce 1000
ibrun -np 1 time_lco_allreduce 10000
ibrun -np 1 time_lco_alltoall 10
ibrun -np 1 time_lco_alltoall 100
ibrun -np 1 time_lco_alltoall 1000
ibrun -np 2 time_lco_netfutures
ibrun -np 2 time_lco_netfutures_msgSize -c 1
~                                              
