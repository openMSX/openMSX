# $Id$

SUBDIRS:=manual

DIST:= \
	release-notes.txt release-history.txt \
	commands.txt debugdevice.txt \
	developersFAQ.txt testcases.txt \
	exampleconfigs.xml \
	vram-addressing.txt \
	openmsx.tex MSX-cassette.dia \
	msxinfo-article.html schema1.png schema2.png screenshot.png

# Backwards compatibility for auto* system:
DIST+=Makefile.am

$(eval $(PROCESS_NODE))

