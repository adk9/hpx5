This folder contains various scripts that can be used to view collected data
related to the steals and steal attempts that occur during an application's 
runtime.  Note that these scripts are designed for SMP runs.  They can be 
extended to work with distributed runs if needed, however.

First, the proper versions of HPCG and HPX must be used.  The branch of HPCG
to use is called 'allreduce-annotations'.  The branch of HPX to use is called
'steal-analysis'.

After compiling HPX and HPCG, run HPCG with the command options
"--hpx-trace-classes=sched", "--hpx-trace-dir=<trace output directory>",
(optionally) "--hpx-trace-buffersize=<size>" (1M or larger should be
sufficient; this will reduce tracing overheads), and "1> annotations.txt".
This will produce both the trace output files in the specified directory
and the file 'annotations.txt' in the directory that hpcg is run in.

Before proceeding further, both of the following commands should be run:
g++ -O3 -o aggregate get_aggregates.cpp
g++ -O3 -o aggregate_normals get_normalized.cpp

Afterwards, copy 'collect_traces.sh' and 'read_log.py' into the trace output
directory and run './collect_traces.sh'.  This will collect all of the trace
data used in the scripts and place them into text files 'dump.???.txt', where
'???' represents the worker number the trace data belongs to.  At present the
'collect_traces.sh' script assumes that 8 workers were used, so if this is not
the case change the end point of the loop ('007') to whatever the highest 
worker number is (e.g. '015' if 16 workers are used).  This step will take a 
long time to complete.

At this point, either 'mv' the 'dump.???.txt' files to where the other scripts
are located or copy the scripts to the directory with the dump files. Also, 
copy 'annotations.txt' to this same directory, as well.  Now,
both 'norm_traces.sh' and 'total_traces.sh' can be run.  The first shell script
will produce several files named 'total.<number>.txt', the second will produce
files named 'norm_total.<number>.txt'.  In both cases, the scripts assume that
8 worker threads were used, and if this is not correct should be changed in the
same way as 'collect_traces.sh'.  Also, both scripts will produce plottable
data with 1 ms intervals between points.  This can be changed by changing the
final argument in the commands that are in the loop of each script (number of
ms).

Finally, the python scripts each take different arguments:  
'delayhist.py' does not require any arguments 
'steal_count.py' takes a list of 'total.<num>.txt' files and plots all of them
'windownorm.py' and 'windownorm2s.py' take a list of 'norm_total.<num>.txt' 
files and plot all of them
