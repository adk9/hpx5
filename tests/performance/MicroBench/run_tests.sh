mpirun perfmain
mpirun -np 2 memget -t 1
mpirun -np 2 memput -t 1
mpirun -np 2 pingpong 10000
