#!/bin/sh

# This script is called by recollindex to determine if it would be
# worth retrying files which previously failed to index.
#
# This is the default implementation, it is pointed to by the
# 'checkneedretryindexscript' variable in the default recoll.conf
#
# The script exits with 0 if retrying should be performed (something
# changed), 1 else.
#
# We check /usr/bin and /usr/local/bin modification date against the
# previous value recorded inside ~/.config/Recoll.org/needidxretrydate
#
# If any argument is given, we record the new state instead of
# generating it (this should be used at the end of an indexing pass
# with retry set).
#

# Bin dirs to be tested:
bindirs="/usr/bin /usr/local/bin $HOME/bin /opt/*/bin"

rfiledir=$HOME/.config/Recoll.org
rfile=$rfiledir/needidxretrydate
nrfile=$rfiledir/tneedidxretrydate

test -d $rfiledir || mkdir -p $rfiledir

# If any argument is given, we are called just to record the new
# state. We do not recompute it as it may have changed during
# indexing, but just move the state in place
if test $# != 0 ; then 
    mv -f $nrfile $rfile
    exit 0
fi

# Compute state of bin dirs and see if anything changed:
> $nrfile
for dir in $bindirs; do
    ls -ld $dir >> $nrfile 2> /dev/null
done

if cmp -s $rfile $nrfile ; then 
    exit 1
else
    exit 0
fi

