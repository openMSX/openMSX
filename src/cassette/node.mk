# $Id$

SRC_HDR:= \
	CassetteDevice \
	CassettePlayer \
	CassettePort \
	DummyCassetteDevice \
	MSXTapePatch \
	DummyCassetteImage \
	CasImage \
	WavImage

HDR_ONLY:= \
	CassetteImage.hh

$(eval $(PROCESS_NODE))

