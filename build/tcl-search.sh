#!/bin/sh
# $Id$
#
# Look for the lowest TCL version >= 8.4.
# Provides an interface similar to lib-config scripts.

BEST_MAJOR_VERSION=9999
BEST_MINOR_VERSION=9999
BEST_CFLAGS=
BEST_LDFLAGS=

for dir in \
	/usr/local/lib64/tcl8.* /usr/lib64/tcl8.* \
	/usr/local/lib64 /usr/lib64 \
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
			BEST_CFLAGS=''
			for FLAG in $TCL_INCLUDE_SPEC
			do
				eval EVALFLAG=$FLAG
				BEST_CFLAGS="${BEST_CFLAGS}${EVALFLAG} "
			done
			if [ -z "$TCL_LIB_SPEC" ]
			then
				# Workaround for MSYS.
				BEST_LDFLAGS=-ltcl$TCL_MAJOR_VERSION$TCL_MINOR_VERSION
			else
				BEST_LDFLAGS=''
				for FLAG in $TCL_LIB_SPEC
				do
					eval EVALFLAG=$FLAG
					BEST_LDFLAGS="${BEST_LDFLAGS}${EVALFLAG} "
				done
			fi
		fi
	#else
	#	echo "  No tclConfig.sh in $dir" 1>&2
	fi
done

if [ $BEST_MAJOR_VERSION -eq 9999 ]
then
	echo "No TCL >= 8.4 found" 1>&2
	exit 1
else
	EXIT=0
	if [ $# -eq 0 ]
	then
		HELP=true
		EXIT=1
	fi
	for option in "$@"
	do
		case "$option" in
			--cflags)
				echo "$BEST_CFLAGS"
				;;
			--ldflags)
				echo "$BEST_LDFLAGS"
				;;
			--version)
				echo "$BEST_MAJOR_VERSION.$BEST_MINOR_VERSION"
				;;
			--help)
				HELP=true
				;;
			*)
				echo "Unknown option: $option" 1>&2
				EXIT=1
				;;
		esac
	done
	if [ "$HELP" = true ]
	then
    	cat <<EOF
Usage: tcl-search.sh [OPTION] ...

Possible values for OPTION are:

  --cflags        print compile flags
  --ldflags       print linker flags
  --help          print this help and exit
  --version       print version information
EOF
	fi
	exit $EXIT
fi
