#!/bin/sh
# $Id$

MYDIR=`dirname $0` || exit
#MYDIR=/usr/share/automake-1.7
#echo "  I'm located in $MYDIR" 1>&2

TIMESTAMP=`$MYDIR/config.guess --time-stamp` || exit
echo "  Using config.guess of $TIMESTAMP..." 1>&2

GUESSED_CONFIG=`$MYDIR/config.guess` || exit
echo "  Detected system: $GUESSED_CONFIG" 1>&2

case "$GUESSED_CONFIG" in
	*-*-*-*)
		GUESSED_CPU=${GUESSED_CONFIG%-*-*-*}
		GUESSED_OS=${GUESSED_CONFIG#*-*-}
		;;
	*-*-*)
		GUESSED_CPU=${GUESSED_CONFIG%-*-*}
		GUESSED_OS=${GUESSED_CONFIG#*-*-}
		;;
	*)
		echo "  Unknown format!" 1>&2
		exit 1
		;;
esac
#echo "  CPU: ${GUESSED_CPU}" 1>&2
#echo "  OS:  ${GUESSED_OS}" 1>&2

case "$GUESSED_CPU" in
	i?86)
		OPENMSX_TARGET_CPU=x86;;
	powerpc)
		OPENMSX_TARGET_CPU=ppc;;
	*)
		echo "  Unknown CPU \"$GUESSED_CPU\"!" 1>&2
		exit 1
		;;
esac
case "$GUESSED_OS" in
	*linux*)
		OPENMSX_TARGET_OS=linux;;
	*darwin*)
		OPENMSX_TARGET_OS=darwin;;
	*freebsd4*)
		OPENMSX_TARGET_OS=freebsd4;;
	*freebsd5*)
		OPENMSX_TARGET_OS=freebsd5;;
	*mingw*)
		OPENMSX_TARGET_OS=mingw32;;
	*)
		echo "  Unknown OS \"$GUESSED_OS\"!" 1>&2
		exit 1
		;;
esac

echo "OPENMSX_TARGET_CPU=$OPENMSX_TARGET_CPU"
echo "OPENMSX_TARGET_OS=$OPENMSX_TARGET_OS"
