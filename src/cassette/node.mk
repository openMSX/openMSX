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
	CassetteImage

$(eval $(PROCESS_NODE))

