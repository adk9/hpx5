#!/bin/bash

for j in {000..007}
do
  ./aggregate dump total $j 1
  echo "done with $j"
done
