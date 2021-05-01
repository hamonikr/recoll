#!/bin/sh

# This is a shell script that starts and stops the recollindex daemon
# depending on whether or not the power supply is plugged in.  It should be
# called from the file ~/.config/autostart/recollindex.desktop.
#
# That is: make the script executable (chmod +x) and replace in
# recollindex.desk the line:
#   Exec=recollindex -w 60 -m 
# With
#   Exec=/path/to/recoll_index_on_ac.sh
#
#
# By: The Doctor (drwho at virtadpt dot net)
# License: GPLv3
# 
# Modifications by J.F Dockes 
#  - replaced "acpi" usage with "on_ac_power" which seems to be both
#    more common and more universal.
#  - Changed the default to be that we run recollindex if we can't determine
#    power status (ie: on_ac_power not installed or not working: we're most
#    probably not running on a laptop). 

INDEXER="recollindex -w 60 -m"
ACPI=`which on_ac_power`

# If the on_ac_power script isn't installed, warn, but run anyway. Maybe
# this is not a laptop or not linux.
if test "x$ACPI" = "x" ; then
    echo "on_ac_power utility not found. Starting recollindex anyway."
fi

while true; do
    # Determine whether or not the power supply is plugged in.
    if test "x$ACPI" != "x" ; then
        on_ac_power
        STATUS=$?
    else
        STATUS=0
    fi

    # Get the PID of the indexing daemon.
    if test -f ~/.recoll/index.pid ; then 
       PID=`cat ~/.recoll/index.pid`
       # Make sure that this is recollindex running. pid could have
       # been reallocated
       ps ax | egrep "^[ \t]*$PID " | grep -q recollindex || PID=""
    fi
#    echo "Recollindex pid is $PID"

    if test $STATUS -eq 1 ; then
	# The power supply is not plugged in.  See if the indexing daemon is
	# running, and if it is, kill it.  The indexing daemon will not be
	# started.
        if test x"$PID" != x; then
	    kill $PID
	fi
    else
	# The power supply is plugged in or we just don't know.  
        # See if the indexing daemon is running, and if it's not start it.
        if test -z "$PID" ; then
	    $INDEXER
	fi
    fi

    # Go to sleep for a while.
    sleep 120
    continue
done
