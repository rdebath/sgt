#!/bin/sh

#
# Make Contact
#
MACHINE=el.mono.org

#
# Check for login barring ....
#

ssh -C -x $MACHINE -l mono 2>/dev/null | tee logfile
