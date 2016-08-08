#!/usr/bin/env python
from matplotlib.pyplot import *
from matplotlib.font_manager import FontProperties
import numpy as np
import sys, random, re

fig = figure(figsize=(16, 6))
    
count = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
hit = 0
data = []
with file("annotations.txt") as f:
  for l in f:
    w = l.split()
    if len(w) == 4:
      kind = int(w[3])
      if kind == 1:
        v = int(w[0])
        t = long(w[1])
        style = int(w[2])
        if hit == 32:
          hit = 0
        elif hit < 16:
          count[hit] = t
        elif hit == 16:
          data.append(float(t - count[0])/1e6)
        hit += 1

vals = np.asarray(data)

xmin = 0
xmax = 5000
y = 4
xtick = 500
results, edges = np.histogram(vals, bins=200, range=(xmin, xmax), normed=True)
binWidth = edges[1] - edges[0]
bar(edges[:-1], results*binWidth*100, binWidth, color='green')

# titles, font, etc.
ylabel('Percentage', size='x-large')
axis([xmin, xmax, 0, y])
#xticks(np.arange(xmin, xmax, xtick))
# yticks(np.arange(n)+0.25, corenum)
#yscale('log', nonposy='clip')
xlabel('Time [ms]', size='x-large')
# tick_params(axis='y', which='major', labelsize=12)
title('Histogram of Delay Between First Allreduce Entrance and Completion', size='xx-large')
font = {'family' : 'normal',
        'weight' : 'bold',
        'size'   : 22}
rc('font', **font)
show()
