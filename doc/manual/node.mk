# $Id$

DIST:= \
	*.html *.css

# Backwards compatibility for auto* system:
DIST+=Makefile.am

$(eval $(PROCESS_NODE))

