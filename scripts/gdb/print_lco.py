#!/usr/bin/env python

### This script is for new gdb command "hpx lco"

import gdb

def validate_args(list_of_args):
  '''Validate the arguments passed to the current command.'''
  
  if len(list_of_args) != 1:
    return 1
    
  temp_sym = gdb.lookup_symbol(list_of_args[0])
  if temp_sym[0] == None:
    return 2
  
  return 0 

def print_lco(lco_name):
  '''Access Local Synchronization Object (LCO) and display it's status. '''

  try:
    #Retrieve hpx_address
    hpx_addr = gdb.parse_and_eval(lco_name)
    #Retrieve LCO type
    temp_str = "symbol_table_find(" + str(hpx_addr) + ")"
    symtab_entry_ptr = gdb.parse_and_eval(temp_str)
    #temp_str = "print *(struct symbol_table*)" + str(symtab_entry_ptr)
    temp_str = "*(struct symbol_table*)(" + str(symtab_entry_ptr) + ")"
    symtab_entry = gdb.parse_and_eval(temp_str)
    temp_addr, cast_to = str(symtab_entry['data_type']).split()
    cast_to = cast_to[1:-1]
    #Use hpx print to display the LCO
    temp_str = "set $req_val = (" + cast_to + " *) malloc(sizeof(" + cast_to \
        + "))"
    gdb.execute(temp_str)
    temp_str = "sizeof(" + cast_to + ")"
    length = gdb.parse_and_eval(temp_str)
    temp_str = "hpx_gas_memget_sync($req_val, " + str(hpx_addr) + ", " + \
        str(length) + ")"
    rs = gdb.parse_and_eval(temp_str)
    temp_str = "print *$req_val"
    gdb.execute(temp_str)
    return 0
  except:
    print("Unable to retrieve LCO status.")

class hpx_lco(gdb.Command):
  """This command displays status of given LCO.
Syntax:
      hpx lco <lco name>"""
  def __init__(self):
    gdb.Command.__init__(self, "hpx lco", gdb.COMMAND_DATA, \
        gdb.COMPLETE_SYMBOL)
  
  def invoke(self, args, from_tty):
    list_of_args = gdb.string_to_argv(args)
    
    rc = validate_args(list_of_args)
    if rc == 1: 
      print ('Invalid arguments. Try "help hpx lco".')
    elif rc == 2:
      print ('Invalid LCO Name.')
    elif rc == 0:
      var = list_of_args[0]
      print_lco(var)

hpx_lco()

