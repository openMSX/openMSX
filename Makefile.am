# $Id$

SUBDIRS = src doc m4

MAINTAINERCLEANFILES = aclocal.m4 Makefile.in \
   config.cache config.guess config.log \
   config.sub configure depcomp install-sh ltmain.sh \
   make.log missing mkinstalldirs

EXTRA_DIST = \
	GPL TODO config-i686.sh config-mingw32.sh \
	Contrib share

# Altough (no)debug only needed in the src dir, we want
# to use the parameters here also
nodebug: resetdebug all
debug: setdebug all

resetdebug:
	@-rm -f src/.debug
setdebug:
	@touch src/.debug


#  if you write a self-test script named `chk', uncomment the
#  following and add `chk' to the EXTRA_DIST list
#TESTS=chk

install-data-hook:
	cp -rv $(srcdir)/share $(prefix)/
	mkdir -p $(prefix)/Contrib/cbios
	cp -v $(srcdir)/Contrib/cbios/*.BIN $(prefix)/Contrib/cbios
##needs find trick because otherwise find tries to descent into rm'ed dir
#	find $(srcdir)/share -name CVS -type d -exec rm -rf $(prefix)/{} \;
##Creating a link for the machine configurations
##TODO: Make this configurable !!
	ln -sf $(prefix)/share/machines/National_CF-1200 $(prefix)/share/machines/msx1
	ln -sf $(prefix)/share/machines/Philips_NMS_8250 $(prefix)/share/machines/msx2
	ln -sf $(prefix)/share/machines/Panasonic_FS-A1FX $(prefix)/share/machines/msx2plus
	ln -sf $(prefix)/share/machines/Panasonic_FS-A1GT $(prefix)/share/machines/turbor
##AutoMake install documentation in share/openmsx, which is not where we want
##it to be. No time now to dig into AutoMake, instead just link the location.
	ln -sf $(prefix)/share/openmsx $(prefix)/doc
##Creating a link for the binary in /usr/local/bin
##TODO: Make this configurable !!
	ln -sf $(prefix)/bin/openmsx /usr/local/bin/openmsx

## a simple cp -R isn't portable so we should use this long winded thing below
## but I need to check if `find` is portabel. code below isn't tested !!!
##
##for i in `find share -type d -print` ; do \
##	  echo i is $$i ; \
#### Compute the install directory at runtime
##	  d=`echo $$i | sed -e s,/[^/]*$$,,`; \
##	  echo d is $$d ; \
#### Make the install directory
##	  $(mkinstalldirs) $(DESTDIR)/$$d; \
#### Find the files
#### "p" is the variable that names the actual file we will install
##	  for p in `find $$i -type f -print` ; do \
##	    echo $$p; \
####	    p=$$f; \
#### actually install the file
##	    $(INSTALL_DATA) $$p $(DESTDIR)/$$f; \
##	  done; \
##	done
