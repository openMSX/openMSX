#!/bin/sh

# $Id$

# Generate configure script
if ( aclocal --version ) </dev/null > /dev/null 2>&1; then
	echo "Building macros"
	aclocal -I m4 || exit
else
	echo "aclocal not found -- aborting"
	exit
fi

if ( autoheader --version ) </dev/null > /dev/null 2>&1; then
	echo "Building config header template"
	autoheader || exit
else
	echo "autoheader not found -- aborting"
	exit
fi

if ( autoconf --version ) </dev/null > /dev/null 2>&1; then
	echo "Building configure"
	autoconf || exit
else
	echo "autoconf not found -- aborting"
	exit
fi

if ( automake --version ) </dev/null > /dev/null 2>&1; then
	echo "Building Makefiles"
	automake --foreign -a || exit
else
	echo "automake not found -- aborting"
	exit
fi

rm -f config.cache
echo 'Next step: run "(g)make"'
