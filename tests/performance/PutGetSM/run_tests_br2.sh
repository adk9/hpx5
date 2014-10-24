DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x

aprun $DIR/perfmain
aprun -n 2 -N 1 -d 16 $DIR/guppie -t 2 10 12
