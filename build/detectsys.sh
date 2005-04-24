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
	x86_64)
		OPENMSX_TARGET_CPU=x86_64;;
	powerpc)
		OPENMSX_TARGET_CPU=ppc;;
	sparc*)
		OPENMSX_TARGET_CPU=sparc;;
	m68k)
		OPENMSX_TARGET_CPU=m68k;;
	alpha*)
		OPENMSX_TARGET_CPU=alpha;;
	arm*)
		OPENMSX_TARGET_CPU=arm;;
	mips)
		OPENMSX_TARGET_CPU=mips;;
	mips*eb)
		OPENMSX_TARGET_CPU=mips;;
	mips*el)
		OPENMSX_TARGET_CPU=mipsel;;
	hppa*)
		OPENMSX_TARGET_CPU=hppa;;
	ia64)
		OPENMSX_TARGET_CPU=ia64;;
	s390*)
		OPENMSX_TARGET_CPU=s390;;
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
	*netbsd*)
		OPENMSX_TARGET_OS=netbsd;;
	*openbsd*)
		OPENMSX_TARGET_OS=openbsd;;
	*mingw*)
		OPENMSX_TARGET_OS=mingw32;;
	*)
		echo "  Unknown OS \"$GUESSED_OS\"!" 1>&2
		exit 1
		;;
esac

echo "OPENMSX_TARGET_CPU=$OPENMSX_TARGET_CPU"
echo "OPENMSX_TARGET_OS=$OPENMSX_TARGET_OS"
