#!/bin/sh
# $Id$

INDIR=${PWD}/build
OUTDIR=${PWD}/derived/autotools

mkdir -p ${OUTDIR}

if ( aclocal --version ) </dev/null > /dev/null 2>&1; then
	echo "Building macros"
	cd ${INDIR} ; aclocal --output=${OUTDIR}/aclocal.m4 -I m4 || exit
else
	echo "aclocal not found -- aborting"
	exit
fi

if ( autoheader --version ) </dev/null > /dev/null 2>&1; then
	echo "Building config header template"
	cd ${OUTDIR} ; autoheader ${INDIR}/configure.ac || exit
else
	echo "autoheader not found -- aborting"
	exit
fi

if ( autoconf --version ) </dev/null > /dev/null 2>&1; then
	echo "Building configure"
	cd ${OUTDIR} ; autoconf --output=configure ${INDIR}/configure.ac || exit
else
	echo "autoconf not found -- aborting"
	exit
fi

echo 'Next step: run "(g)make"'
