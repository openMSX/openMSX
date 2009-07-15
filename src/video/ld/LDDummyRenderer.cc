// $Id$

#include "LDDummyRenderer.hh"
#include "DisplayMode.hh"

namespace openmsx {

void LDDummyRenderer::reset(EmuTime::param /*time*/) {
}

void LDDummyRenderer::frameStart(EmuTime::param /*time*/) {
}

void LDDummyRenderer::frameEnd(EmuTime::param /*time*/) {
}

void LDDummyRenderer::paint(OutputSurface& /*output*/) {
}

void LDDummyRenderer::drawBlank(int /*r*/, int /*g*/, int /*b*/) {
}

void LDDummyRenderer::drawBitmap(byte * /*frame*/) {
}

const std::string& LDDummyRenderer::getName() {
	static const std::string NAME = "LDDummyRenderer";
	return NAME;
}

} // namespace openmsx
