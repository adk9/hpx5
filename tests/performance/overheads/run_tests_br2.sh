DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x

aprun $DIR/perfmain
aprun $DIR/time_gas_alloc
aprun -n 2 -N 1 -d 16 $DIR/time_gas_addr_trans -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20))
aprun -n 2 -N 1 -d 16 $DIR/time_lco_chan -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20))
aprun -n 2 -N 1 -d 16 $DIR/time_lco_sema -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20))
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_future -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20))
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allgather -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 10
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allgather -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 100
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allgather -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 1000
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allreduce -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 10
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allreduce -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 100
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_allreduce -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 1000
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_alltoall -c 16 -T 0 -s 4097 -p $((1024 * 1024* 1024 * 20)) 10
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_alltoall -c 16 -T 0 -s 4097 -p $((1024 * 1024* 1024 * 20)) 100
aprun  -n 2 -N 1 -d 16 $DIR/time_lco_alltoall -c 16 -T 0 -s 4097 -p $((1024 * 1024* 1024 * 20)) 1000
#aprun -n 2 -N 1 -d 16 $DIR/time_lco_netfutures -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20))
