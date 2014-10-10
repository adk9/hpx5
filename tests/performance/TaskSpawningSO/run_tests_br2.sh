aprun perfmain
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 15
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 20
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 25
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 30
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 35
aprun -n 2 -N 1 -d 32 fibonacci -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 40
aprun -n 2 -N 1 -d 32 parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 1000000
aprun -n 2 -N 1 -d 32 parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 2000000
aprun -n 2 -N 1 -d 32 parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 3000000
aprun -n 2 -N 1 -d 32 parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 4000000
aprun -n 2 -N 1 -d 32 parspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 5000000
aprun -n 2 -N 1 -d 32 seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 1000000
aprun -n 2 -N 1 -d 32 seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 2000000
aprun -n 2 -N 1 -d 32 seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 3000000
aprun -n 2 -N 1 -d 32 seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 4000000
aprun -n 2 -N 1 -d 32 seqspawn -c 16 -T 0 -s 4097 -p $((1024 * 1024 * 1024 * 50)) 5000000
