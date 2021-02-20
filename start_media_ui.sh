#!/usr/bin/env bash

LOCKFILE="/tmp/media_ui.lock"
PROGRAM=$(echo ~/flhu/headunit/media_ui/media_ui)

# Remove the lockfile before killing the software
rm -f $LOCKFILE
if [ ! -z "$(pidof `basename $PROGRAM`)" ]; then
    killall -9 $(basename $PROGRAM)
fi

if [ -x "$PROGRAM" ]; then
    touch "$LOCKFILE"
    
#SILENT VERSION    until "$PROGRAM" > /dev/null 2<&1; do
    until "$PROGRAM"; do
	
	if [ ! -f $LOCKFILE ]; then
	    exit 0
	fi
	
	echo "media_ui crashed with exit code $?. Respawning.." >&2
	sleep 1s	
    done
else
    echo "ERROR: $PROGRAM program is missing."
fi

exit 0
