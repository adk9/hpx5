#!/usr/bin/env python

### This script is for new gdb catchpoint to handle signals emitted by HPX

catch signal SIGUSR1
commands
silent
handle SIGUSR1 nostop
cont
end


