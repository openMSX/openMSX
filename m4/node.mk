# $Id$

DIST:=*.m4

# Backwards compatibility for auto* system:
DIST+=Makefile.am

$(eval $(PROCESS_NODE))

