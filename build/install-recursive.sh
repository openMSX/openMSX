#!/bin/sh
# $Id$

if [ $# -lt 2 ]
then
	echo "Usage: $0 (<src-file>|<src-dir>)+ <dst-dir>" >&2
	exit 1
fi

if [ $# -eq 2 ]
then
	src="$1"
	shift
	if [ -d "$src" ]
	then
		src="$src"/*
	fi
else
	src=""
	while [ $# -ne 1 ]
	do
		src="$src $1"
		shift
	done
fi
dst="$1"

for path in $src
do
	name=$(basename "$path")
	dir=$(dirname "$path")
	if [ -L "$path" ]
	then
		echo "skipping symbolic link: $path"
	elif [ -d "$path" ]
	then
		if [ "$name" != .svn ]
		then
			$0 "$path" "$dst"
		fi
	else
		install -m 0755 -d "$dst/$dir"
		mode=0644
		if [ -x "$path" ]
		then
			mode=0755
		fi
		install -m $mode "$path" "$dst/$dir"
	fi
done
