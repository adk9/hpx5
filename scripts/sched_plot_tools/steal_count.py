#!/usr/bin/env python
from matplotlib.pyplot import *
from matplotlib.font_manager import FontProperties
import numpy as np
import sys, random, re

fig = figure(figsize=(16, 6))
data = []
colors = ['#0000FF',
          '#00FF00',
          '#00FFFF', 
          '#FF0000', 
          '#FF00FF', 
          '#FFFF00', 
          '#E1E1E1', 
          '#000000', 
          '#888888', 
          '#000088', 
          '#008800', 
          '#008888', 
          '#880000', 
          '#880088', 
          '#888800', 
          '#1234FF'

]
corenum=[]
    
for i, fn in enumerate(sys.argv[1:]):
    num = re.search(r'\d+', fn).group()
    corenum += [num]
    count = 0;
    with file(fn) as f:
        x = []
        y = []
        for l in f:
            w = l.split()
            if len(w) != 2:
                raise 'invalid entry'
            t, c = float(w[0]), long(w[1])
            count += c;
            x = x + [t]
            y = y + [count]
        plot(x, y, color=colors[i])
       
t = 0
found = 0
width = 1
style = 0
count = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
with file("noneannotations.txt") as f:
  for l in f:
    w = l.split()
    if len(w) == 4:
      v = int(w[0])
      style = int(w[2])
      if corenum.count(str(v)) > 0 and int(w[3]) != 0:
        col = colors[count[v] % 16]
        if style == 0:
          axvline(x=long(w[1]) / 1e9, color=col, linestyle='solid')
        elif style == 1:
          axvline(x=long(w[1]) / 1e9, color=col, linestyle='dashed')
          count[v] += 1
        elif style == 2:
          axvline(x=long(w[1]) / 1e9, color=col, linestyle='dashdot')
        elif style == 3:
          axvline(x=long(w[1]) / 1e9, color=col, linestyle='dotted')
          

# titles, font, etc.
ylabel('Total Attempted Steal Count', size='x-large')
xlabel('Time [seconds]', size='x-large')
tick_params(axis='both', which='major', labelsize=12)
suptitle('Cumulative Attempted Steal Count by Worker Thread in HPCG', size='xx-large')
title('With Annotations of Allreduce Phases Color coded by Occurrence', size='xx-large')
show()
