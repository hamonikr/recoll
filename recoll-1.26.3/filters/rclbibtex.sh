#!/bin/sh
# Remove unwanted data from bibtex file by deleting "bad" lines and
# cleaning "good" ones. 

if test "X$RECOLL_FILTER_FORPREVIEW" = "Xyes" ; then
    sed \
        -e '/^$/N;/^\n$/D' \
        -e '/(bibdsk.*\|month\|bibsource\|crossref\|ee\|groups\|owner\|pages\|timestamp\|url\|file\|price\|citeulike.*\|markedentry\|posted-at)[[:space:]=]*/I d' \
        -e '/@[^{]*{/ d' \
        \
        -e 's/{//g' \
        -e 's/},\?$//g' \
        -e 's/^[ 	]*//' \
        -e 's/({|}|=)//g' \
        < $1
else
    sed -e '/\(StaticGroup\|bibdsk.*\|month\|edition\|address\|query\|bibsource\|crossref\|ee\|groups\|owner\|pages\|timestamp\|url\|file\|price\|citeulike.*\|markedentry\|posted-at\)[[:space:]=]*/I d' \
        -e '/@[^{]*{/ d' \
        \
        -e 's/^[^{]*{//g' \
        -e 's/},\?$//g' \
        -e 's/({|}|=)//g' \
        < $1
fi
