#include "DummyRenderer.hh"

#include "DisplayMode.hh"

namespace openmsx {

PostProcessor* DummyRenderer::getPostProcessor() const {
	return nullptr;
}

void DummyRenderer::reInit() {
}

void DummyRenderer::frameStart(EmuTime /*time*/) {
}

void DummyRenderer::frameEnd(EmuTime /*time*/) {
}

void DummyRenderer::updateTransparency(bool /*enabled*/, EmuTime /*time*/) {
}

void DummyRenderer::updateSuperimposing(const RawFrame* /*videoSource*/,
                                        EmuTime /*time*/) {
}

void DummyRenderer::updateForegroundColor(uint8_t /*color*/, EmuTime /*time*/) {
}

void DummyRenderer::updateBackgroundColor(uint8_t /*color*/, EmuTime /*time*/) {
}

void DummyRenderer::updateBlinkForegroundColor(uint8_t /*color*/, EmuTime /*time*/) {
}

void DummyRenderer::updateBlinkBackgroundColor(uint8_t /*color*/, EmuTime /*time*/) {
}

void DummyRenderer::updateBlinkState(bool /*enabled*/, EmuTime /*time*/) {
}

void DummyRenderer::updatePalette(unsigned /*index*/, int /*grb*/, EmuTime /*time*/) {
}

void DummyRenderer::updateVerticalScroll(int /*scroll*/, EmuTime /*time*/) {
}

void DummyRenderer::updateHorizontalScrollLow(uint8_t /*scroll*/, EmuTime /*time*/) {
}

void DummyRenderer::updateHorizontalScrollHigh(uint8_t /*scroll*/, EmuTime /*time*/) {
}

void DummyRenderer::updateBorderMask(bool /*masked*/, EmuTime /*time*/) {
}

void DummyRenderer::updateMultiPage(bool /*multiPage*/, EmuTime /*time*/) {
}

void DummyRenderer::updateHorizontalAdjust(int /*adjust*/, EmuTime /*time*/) {
}

void DummyRenderer::updateDisplayEnabled(bool /*enabled*/, EmuTime /*time*/) {
}

void DummyRenderer::updateDisplayMode(DisplayMode /*mode*/, EmuTime /*time*/) {
}

void DummyRenderer::updateNameBase(unsigned /*addr*/, EmuTime /*time*/) {
}

void DummyRenderer::updatePatternBase(unsigned /*addr*/, EmuTime /*time*/) {
}

void DummyRenderer::updateColorBase(unsigned /*addr*/, EmuTime /*time*/) {
}

void DummyRenderer::updateSpritesEnabled(bool /*enabled*/, EmuTime /*time*/) {
}

void DummyRenderer::updateVRAM(unsigned /*offset*/, EmuTime /*time*/) {
}

void DummyRenderer::updateWindow(bool /*enabled*/, EmuTime /*time*/) {
}

void DummyRenderer::paint(OutputSurface& /*output*/) {
}

} // namespace openmsx
