// $Id$

#include "DummyRenderer.hh"
#include "DisplayMode.hh"

namespace openmsx {

void DummyRenderer::reset(const EmuTime& /*time*/) {
}

void DummyRenderer::frameStart(const EmuTime& /*time*/) {
}

void DummyRenderer::frameEnd(const EmuTime& /*time*/) {
}

void DummyRenderer::updateTransparency(bool /*enabled*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateForegroundColour(int /*colour*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateBackgroundColour(int /*colour*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateBlinkForegroundColour(int /*colour*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateBlinkBackgroundColour(int /*colour*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateBlinkState(bool /*enabled*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updatePalette(int /*index*/, int /*grb*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateVerticalScroll(int /*scroll*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateHorizontalScrollLow(byte /*scroll*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateHorizontalScrollHigh(byte /*scroll*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateBorderMask(bool /*masked*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateMultiPage(bool /*multiPage*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateHorizontalAdjust(int /*adjust*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateDisplayEnabled(bool /*enabled*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateDisplayMode(DisplayMode /*mode*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateNameBase(int /*addr*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updatePatternBase(int /*addr*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateColourBase(int /*addr*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateSpritesEnabled(bool /*enabled*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateVRAM(unsigned /*offset*/, const EmuTime& /*time*/) {
}

void DummyRenderer::updateWindow(bool /*enabled*/, const EmuTime& /*time*/) {
}

void DummyRenderer::paint() {
}

const std::string& DummyRenderer::getName() {
	static const std::string NAME = "DummyRenderer";
	return NAME;
}

} // namespace openmsx

