#!/bin/sh
# $Id$

if [ $# -lt 3 ]
then
	echo "Usage: $0 <base> (<src-file>|<src-dir>)+ <dst-dir>" >&2
	exit 1
fi

base="$1"
shift

src="$base/$1"
shift
if [ $# -eq 1 ]
then
	if [ -d "$src" ]
	then
		src="$src"/*
	fi
else
	while [ $# -ne 1 ]
	do
		src="$src $base/$1"
		shift
	done
fi
dst="$1"

for path in $src
do
	name=`basename "$path"`
	dir=`dirname "${path#$base/}"`
	if [ -L "$path" ]
	then
		echo "skipping symbolic link: $path"
	elif [ -d "$path" ]
	then
		if [ "$name" != .svn ]
		then
			$0 "$base" "${path#$base/}" "$dst"
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
