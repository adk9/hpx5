#!/usr/bin/env python

### This script is for new gdb command "hpx next"

import gdb

def finish_parcel():
  '''Finish execution of current parcel.'''
  temp_str = "handle SIGUSR1 stop"
  gdb.execute(temp_str)
  gdb.execute("continue")
  return 0

class next_parcel(gdb.Command):
  """This command continues execution until end of current parcel."""
  def __init__(self):
    gdb.Command.__init__(self, "hpx next", gdb.COMMAND_RUNNING, \
        gdb.COMPLETE_SYMBOL)
  
  def invoke(self, args, from_tty):
    finish_parcel()

next_parcel()

