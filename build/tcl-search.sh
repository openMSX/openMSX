#!/bin/sh
# $Id$
#
# Look for the lowest TCL version >= 8.4.

BEST_MAJOR_VERSION=9999
BEST_MINOR_VERSION=9999
BEST_CXXFLAGS=
BEST_LDFLAGS=

for dir in \
	/usr/local/lib/tcl8.* /usr/lib/tcl8.* \
	/usr/local/lib /usr/lib \
	/usr/local/bin /usr/bin \
	`echo $PATH | sed -n "s/:/ /gp;s/\/bin/\/lib/gp"`
do
	if [ -r $dir/tclConfig.sh ]
	then
		TCL_MAJOR_VERSION=0
		TCL_MINOR_VERSION=0
		. $dir/tclConfig.sh
		REMEMBER=false
		if [ $TCL_MAJOR_VERSION -ge 8 ]
		then
			if [ $TCL_MINOR_VERSION -ge 4 ]
			then
				#echo "  Found useful TCL $TCL_VERSION in $dir" 1>&2
				if [ $TCL_MAJOR_VERSION -lt $BEST_MAJOR_VERSION ]
				then
					REMEMBER=true
				else
					if [ $TCL_MAJOR_VERSION -eq $BEST_MAJOR_VERSION ]
					then
						if [ $TCL_MINOR_VERSION -lt $BEST_MINOR_VERSION ]
						then
							REMEMBER=true
						fi
					fi
				fi
			#else
			#	echo "  Found too old TCL $TCL_VERSION in $dir" 1>&2
			fi
		#else
		#	echo "  Found wrong major version TCL $TCL_VERSION in $dir" 1>&2
		fi
		if [ "$REMEMBER" = true ]
		then
			BEST_MAJOR_VERSION=$TCL_MAJOR_VERSION
			BEST_MINOR_VERSION=$TCL_MINOR_VERSION
			eval BEST_CXXFLAGS=$TCL_INCLUDE_SPEC
			if [ -z "$TCL_LIB_FLAG" ]
			then
				# Workaround for MSYS.
				BEST_LDFLAGS=-ltcl$TCL_MAJOR_VERSION$TCL_MINOR_VERSION
			else
				eval BEST_LDFLAGS=$TCL_LIB_FLAG
			fi
		fi
	#else
	#	echo "  No tclConfig.sh in $dir" 1>&2
	fi
done

if [ $BEST_MAJOR_VERSION -eq 9999 ]
then
	echo "No TCL >= 8.4 found" 1>&2
	echo "TCL_FOUND := false"
else
	echo "Found TCL $BEST_MAJOR_VERSION.$BEST_MINOR_VERSION" 1>&2
	echo "TCL_CXXFLAGS := $BEST_CXXFLAGS"
	echo "TCL_LDFLAGS := $BEST_LDFLAGS"
	echo "TCL_FOUND := true"
fi
