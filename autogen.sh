#!/bin/sh
# $Id$

# Force new autoconf on Gentoo.
# (autodetect fails because output dir does not contain any trigger files)
export WANT_AUTOCONF=2.5

INDIR=${PWD}/build
OUTDIR=${PWD}/derived/autotools

mkdir -p ${OUTDIR}

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
