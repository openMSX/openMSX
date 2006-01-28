# $Id$
#
# Create an application directory for Darwin.

# This Makefile needs parameters to operate; check that they were specified:
# - output directory
ifeq ($(OUTDIR),)
$(error Missing parameter: OUTDIR)
endif
# - executable binary
ifeq ($(BINARY),)
$(error Missing parameter: BINARY)
endif

include build/version.mk

.PHONY: all app

all: app

APPDIR:=$(OUTDIR)/openMSX.app
SUPPORTDIR:=build/package-darwin

APPEXE:=$(APPDIR)/Contents/MacOS/openmsx
APPPLIST:=$(APPDIR)/Contents/Info.plist
APPRES:=$(APPDIR)/Contents/Resources
APPICON:=$(APPRES)/openmsx-logo.icns
APPSHARE:=$(APPDIR)/share

app: $(APPEXE) $(APPPLIST) $(APPICON) $(APPSHARE)

$(APPEXE): $(BINARY)
	@echo "  Copying executable..."
	@mkdir -p $(@D)
	@cp $< $@

$(APPPLIST): $(APPDIR)/Contents/%: $(SUPPORTDIR)/%
	@echo "  Writing meta-info..."
	@mkdir -p $(@D)
	@sed -e 's/%ICON%/$(notdir $(APPICON))/' \
		-e 's/%VERSION%/$(PACKAGE_VERSION)/' < $< > $@
	@echo "APPLoMSX" > $(@D)/PkgInfo

$(APPICON): $(APPRES)/%: $(SUPPORTDIR)/%
	@echo "  Copying resources..."
	@mkdir -p $(@D)
	@cp $< $@

$(APPSHARE): share
	@echo "  Copying openMSX data files..."
	@mkdir -p $@
	@sh build/install-recursive.sh share $@
