#!/bin/sh
# $Id$

if [ $# -ne 2 ]
then
	echo "Usage: $0 <src-dir> <dst-dir>" >&2
	exit 1
fi

src=$1
dst=$2

if [ ! -d $src ]
then
	echo "Source argument is not a directory: $src" >&2
	exit 1
fi

install -d $dst
files=
for path in $src/*
do
	name=$(basename $path)
	if [ -d $src/$name ]
	then
		if [ $name != CVS ]
		then
			$0 $src/$name $dst/$name
		fi
	else
		files="$files $src/$name"
	fi
done
if [ -n "$files" ]
then
	install -m 0644 $files $dst
fi
