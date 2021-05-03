#!/bin/sh
# Copyright  (C) 2006 J.F.Dockes
#######################################################
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
######################################################

###################
# Start/stop a recollindex program running as permanent real time indexer.
# The running program writes its pid in $RECOLL_CONFDIR/index.pid
# The portability of this script has not been fully tested.
#

fatal()
{
    echo $*
    exit 1
}
usage()
{
    fatal "Usage: rclmon.sh <start|stop>"
}

test $# -eq 1 || usage

export LANG=C

RECOLL_CONFDIR=${RECOLL_CONFDIR:-$HOME/.recoll}
#echo RECOLL_CONFDIR = ${RECOLL_CONFDIR}

pidfile="${RECOLL_CONFDIR}/index.pid"
opid=0


if test -f $pidfile ; then
   read opid junk < $pidfile
fi

if test $opid -gt 0; then
   out=`kill -0 ${opid} 2>&1`
   if test $? -ne 0 ; then
      if test `expr "$out" : '.*such *process.*'` -ne 0 ; then	
	 opid=0
      else 
	 fatal cant test existence of running process 
      fi
   fi
fi

#echo "Existing pid $opid"

case $1 in
     start)
	if test "$opid" -ne 0 ; then
	   fatal "Already running process: $opid"
	fi
	recollindex -m
     ;;
     stop)
	if test "$opid" -eq 0 ; then
	   fatal "No process running"
	fi
	kill $opid
     ;;
     *)
     usage
esac

