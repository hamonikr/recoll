#!/bin/sh
fatal()
{
    echo $*
    exit 1
}

commoncode=recfiltcommon
test -f recfiltcommon || fatal must be executed inside the filters directory

for filter in rcl* ; do 
sed -e '/^#RECFILTCOMMONCODE/r recfiltcommon
/^#RECFILTCOMMONCODE/,/^#ENDRECFILTCOMMONCODE/d
' < $filter > filtertmp && mv -f filtertmp $filter && chmod a+x $filter
done
