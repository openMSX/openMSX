// $Id$

#include "DummyRenderer.hh"
#include "DisplayMode.hh"

namespace openmsx {

void DummyRenderer::reset(EmuTime::param /*time*/) {
}

void DummyRenderer::frameStart(EmuTime::param /*time*/) {
}

void DummyRenderer::frameEnd(EmuTime::param /*time*/) {
}

void DummyRenderer::updateTransparency(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateForegroundColour(int /*colour*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBackgroundColour(int /*colour*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkForegroundColour(int /*colour*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkBackgroundColour(int /*colour*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkState(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updatePalette(int /*index*/, int /*grb*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateVerticalScroll(int /*scroll*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateHorizontalScrollLow(byte /*scroll*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateHorizontalScrollHigh(byte /*scroll*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBorderMask(bool /*masked*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateMultiPage(bool /*multiPage*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateHorizontalAdjust(int /*adjust*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateDisplayEnabled(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateDisplayMode(DisplayMode /*mode*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateNameBase(int /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updatePatternBase(int /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateColourBase(int /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateSpritesEnabled(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateVRAM(unsigned /*offset*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateWindow(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::paint(OutputSurface& /*output*/) {
}

const std::string& DummyRenderer::getName() {
	static const std::string NAME = "DummyRenderer";
	return NAME;
}

} // namespace openmsx
