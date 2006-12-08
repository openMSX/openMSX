# $Id$

include build/node-start.mk

SRC_HDR:= \
	CassetteDevice \
	CassettePlayer \
	CassettePlayerCLI \
	CassettePort \
	DummyCassetteDevice \
	CassetteImage \
	CasImage \
	WavImage

SRC_HDR_$(COMPONENT_JACK)+= \
	CassetteJack


include build/node-end.mk

