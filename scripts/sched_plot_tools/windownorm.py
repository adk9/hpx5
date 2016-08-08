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
    
region = .05
inc = .001
start = int(region / inc)
start2 = 2 * start
for i, fn in enumerate(sys.argv[1:]):
    num = re.search(r'\d+', fn).group()
    corenum += [num]
    with file(fn) as f:
        x = []
        x1 = []
        y = []
        y1 = []
        y2 = []
        count = [0, 0]
        for l in f:
            w = l.split()
            if len(w) != 4:
                raise 'invalid entry'
            c, w = int(w[1]), int(w[3])
            y1 = y1 + [c]
            y2 = y2 + [w]

        stop = len(y1) - start
        index = 0
        while index < start2:
          count[0] = count[0] + y1[index]
          count[1] = count[1] + y2[index]
          index += 1
        if count[1] == 0:
          y = y + [float('nan')]
        else:
          y = y + [count[0]/count[1]]
        x = x + [inc*(index-start)]
        while index < len(y1):
          count[0] = count[0] + y1[index] - y1[index - start2]
          count[1] = count[1] + y2[index] - y2[index - start2]
          index += 1
          x = x + [inc*(index-start)]
          if count[1] == 0:
            y = y + [float('nan')]
          else:
            y = y + [count[0]/count[1]]
        plot(x, y, color=colors[i])
            
# titles, font, etc.
ylabel('Steal Attempts per Successful Steal', size='x-large')
xlabel('Time [seconds]', size='x-large')
tick_params(axis='both', which='major', labelsize=12)
suptitle('Windowed (.1 s) Average of Steal Attempts per Steal by Worker Thread in HPCG', size='xx-large')
show()
