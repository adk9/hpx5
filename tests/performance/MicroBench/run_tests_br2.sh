aprun perfmain
aprun -n 2 -N 1 -d 16 ./memget -c 1 -T 0
aprun -n 2 -N 1 -d 16 ./memput -c 1 -T 0
aprun -n 2 -N 1 -d 16 ./pingpong -c 1 -T 0 10000
