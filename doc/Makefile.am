# $Id$

SUBDIRS = manual

#  install the man pages
# Man page is broken, so I disabled it.
#man_MANS = \
#	openmsx.1

pkgdata_DATA = \
	release-notes.txt release-history.txt \
	commands.txt debugdevice.txt \
	developersFAQ.txt testcases.txt \
	exampleconfigs.xml \
	vram-addressing.txt \
	openmsx.tex MSX-cassette.dia \
	msxinfo-article.html schema1.png schema2.png screenshot.png

EXTRA_DIST = $(man_MANS) $(pkgdata_DATA)

MAINTAINERCLEANFILES = Makefile.in
