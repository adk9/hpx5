#!/usr/bin/expect -f

set USER     ""
set PASSWORD ""

if {$argc == 2} {
  set USER [lindex $argv 0]
  set arg1 [lindex $argv 1]
  set fp [open $arg1 r]
  set PASSWORD [read $fp]
}

spawn ssh -A -t $USER@crcfe02.crc.nd.edu ssh -A -t cswarmfe.crc.nd.edu
expect "password: "
send "$PASSWORD\r"
expect "password: "
send "$PASSWORD\r"

interact
