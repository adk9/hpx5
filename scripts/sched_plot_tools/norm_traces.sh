#!/bin/bash

  for j in {000..007}
  do
    echo "starting $j"
    ./aggregate_normals dump norm_total $j 1
  done
