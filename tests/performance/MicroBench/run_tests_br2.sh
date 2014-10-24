DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x

aprun $DIR/perfmain
aprun -n 2 -N 1 -d 16 $DIR/memget -c 1 -T 0
aprun -n 2 -N 1 -d 16 $DIR/memput -c 1 -T 0
aprun -n 2 -N 1 -d 16 $DIR/pingpong -c 1 -T 0 10000
