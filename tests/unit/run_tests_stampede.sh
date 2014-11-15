DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

ibrun -np 2 $DIR/hpxtest
