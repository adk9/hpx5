ibrun perfmain
ibrun -np 2 memget -t 1
ibrun -np 2 memput -t 1
ibrun -np 2 pingpong 10000
