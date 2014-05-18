#!/bin/sh
for name in python python2 python2.6 python2.7 python2.8 python2.9
do
	$name -c 'import sys; sys.exit(not((2, 6) <= sys.version_info < (3, )))' \
		2> /dev/null
	if test $? -eq 0
	then
		echo $name
		break
	fi
done
