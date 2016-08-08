#!/bin/bash

for i in {000..007}
do
rm dump${i}.txt
echo "EVENT_SCHED_TRANSFER" >> dump${i}.txt
echo 33
python -c "import read_log;\
read_log.print_file(\"event.${i}.033.00000.SCHED_TRANSFER.log\");\
quit()" >> dump${i}.txt
echo "EVENT_SCHED_STEAL_BEGIN" >> dump${i}.txt
echo 31
python -c "import read_log;\
read_log.print_file(\"event.${i}.031.00000.SCHED_STEAL_BEGIN.log\");\
quit()" >> dump${i}.txt
echo "EVENT_SCHED_STEAL_END" >> dump${i}.txt
echo 32
python -c "import read_log;\
read_log.print_file(\"event.${i}.032.00000.SCHED_STEAL_END.log\");\
quit()" >> dump${i}.txt
echo "done with $i"
done
