#!/bin/csh

make ARCH=mira-opt clean
rm gpu_setup.o
make ARCH=mira-opt

