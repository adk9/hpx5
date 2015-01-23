#!/bin/bash

echo Killing my job

module load maui
job_ids=( $(showq | grep crest-team | awk '{print $1}') )

for var in "${job_ids[@]}"
do
  echo "${var}"
  # do something on $var
  qdel "${var}"
done


touch /tmp/killing.txt
module unload maui
