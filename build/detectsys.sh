#!/bin/sh
# $Id$

MYDIR=`dirname $0` || exit
GUESSED_CONFIG=`$MYDIR/config.guess` || exit

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
		echo "Cannot parse system name \"$GUESSED_CONFIG\"!" 1>&2
		exit 1
		;;
esac
case "$GUESSED_CPU" in
	i?86)
		OPENMSX_TARGET_CPU=x86;;
	x86_64)
		OPENMSX_TARGET_CPU=x86_64;;
	amd64)
		OPENMSX_TARGET_CPU=x86_64;;
	powerpc)
		OPENMSX_TARGET_CPU=ppc;;
	powerpc64)
		OPENMSX_TARGET_CPU=ppc64;;
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
		echo "Unknown CPU \"$GUESSED_CPU\"!" 1>&2
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
	*kfreebsd*)
		OPENMSX_TARGET_OS=kfreebsd;;
	*freebsd*)
		OPENMSX_TARGET_OS=freebsd;;
	*netbsd*)
		OPENMSX_TARGET_OS=netbsd;;
	*openbsd*)
		OPENMSX_TARGET_OS=openbsd;;
	*mingw*)
		OPENMSX_TARGET_OS=mingw32;;
	*)
		echo "Unknown OS \"$GUESSED_OS\"!" 1>&2
		exit 1
		;;
esac

echo "$OPENMSX_TARGET_CPU-$OPENMSX_TARGET_OS"
