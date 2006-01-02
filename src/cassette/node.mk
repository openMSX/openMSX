# $Id$

include build/node-start.mk

SRC_HDR:= \
	CassetteDevice \
	CassettePlayer \
	CassettePort \
	DummyCassetteDevice \
	DummyCassetteImage \
	CassetteImage \
	CasImage \
	WavImage

SRC_HDR_$(COMPONENT_JACK)+= \
	CassetteJack


include build/node-end.mk

