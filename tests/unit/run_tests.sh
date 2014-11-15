DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mpirun -np 2 $DIR/hpxtest
