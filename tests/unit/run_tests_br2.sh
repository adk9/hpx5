DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

aprun -n 2 -N 1 -d 16 $DIR/hpxtest -c 1 -T 0
