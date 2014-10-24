DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x

aprun $DIR/perfmain
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 15
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 20
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 25
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 30
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 35
aprun -n 2 -N 1 -d 16 $DIR/fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 40
aprun -n 2 -N 1 -d 16 $DIR/parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 1000000
aprun -n 2 -N 1 -d 16 $DIR/parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 2000000
aprun -n 2 -N 1 -d 16 $DIR/parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 3000000
aprun -n 2 -N 1 -d 16 $DIR/parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 4000000
aprun -n 2 -N 1 -d 16 $DIR/parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 5000000
aprun -n 2 -N 1 -d 16 $DIR/seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 1000000
aprun -n 2 -N 1 -d 16 $DIR/seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 2000000
aprun -n 2 -N 1 -d 16 $DIR/seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 3000000
aprun -n 2 -N 1 -d 16 $DIR/seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 4000000
aprun -n 2 -N 1 -d 16 $DIR/seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 20)) 5000000
