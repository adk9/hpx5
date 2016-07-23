#!/usr/bin/env python

### This script is for new gdb command "info parcels"

import gdb

def print_parcels():
  try:
    ### Retrieve Action Table entries
    actions = gdb.parse_and_eval('actions')

    ### Display parcels for current rank.
    print ("--------------------\nScheduler Parcels:\n--------------------")
    ### Retrieve Worker Threads
    number_of_workers = gdb.parse_and_eval('here->sched->n_workers')
    print ("Total number of worker threads: %d" % number_of_workers)
    workers = gdb.parse_and_eval('here->sched->workers')
    
    ### Loop through each worker thread and print all the parcels
    for i in range(0, int(number_of_workers)):
      print ("--------------------\nWorker %d info:\n--------------------" % i)
      worker = workers[i]
    
      ### Retrieve current parcel
      current = worker['current']
      print ("Current parcel address: %s" % current)
      current_parcel = current.dereference()
      action_nbr = current_parcel['action']
      action_str = str(actions[action_nbr]['key'])
      temp_index1 = action_str.find("\"") + 1
      temp_index2 = action_str.find("\"", temp_index1)
      action_name = action_str[temp_index1:temp_index2]
      print ("Current action: %s " % action_name)
      size = current_parcel['size']
      print ("Parcel size: %d" % size)
      data_ptr = current_parcel['buffer']
      print ("Buffer address: %s" % data_ptr)

      ### Process work queue for current worker thread
     queues = worker['queues']
     for m in range(2):
       queue = queues[m]
       work_queue = queue['work']
       bottom = work_queue['bottom']
       top = work_queue['top']
       queue_buffer = work_queue['buffer']
       mask = queue_buffer['mask']
       queued_actions = queue_buffer['buffer']
       print("-------------------")
       print("Parcels in Queue %d" % m)
       print("-------------------")
       if top == bottom:
         print("Queue is empty")
         continue
       print("Parcel Address\t\tAction")
       print("--------------\t\t-----------------------")
       for j in range(int(top), int(bottom)):
         k = j&mask
         parcel_ptr = queued_actions[k]
         temp_string3 = '*(struct hpx_parcel *)' + str(parcel_ptr)
         parcel = gdb.parse_and_eval(temp_string3)
         action_nbr = parcel['action']
         action_str = str(actions[action_nbr]['key'])
         temp_index1 = action_str.find("\"") + 1
         temp_index2 = action_str.find("\"", temp_index1)
         action_name = action_str[temp_index1:temp_index2]
         print(str(parcel_ptr) + "\t\t" + action_name)
  except:
    print("Unable to retrieve Parcels information.")

class hpx_parcels(gdb.Command):
  '''This command prints all the parcels that are currently in execution.'''
  def __init__(self):
    gdb.Command.__init__(self, "info parcels", gdb.COMMAND_DATA)
  
  def invoke(self, arg, from_tty):
    print_parcels()
 
hpx_parcels()
