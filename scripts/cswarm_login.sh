#!/usr/bin/expect -f

set USER     ""
set PASSWORD ""

spawn ssh -A -t $USER@crcfe02.crc.nd.edu ssh -A -t cswarmfe.crc.nd.edu
expect "password: "
send "$PASSWORD\r"
expect "password: "
send "$PASSWORD\r"

interact
