#include "DummyRenderer.hh"
#include "DisplayMode.hh"

namespace openmsx {

PostProcessor* DummyRenderer::getPostProcessor() const {
	return nullptr;
}

void DummyRenderer::reInit() {
}

void DummyRenderer::frameStart(EmuTime::param /*time*/) {
}

void DummyRenderer::frameEnd(EmuTime::param /*time*/) {
}

void DummyRenderer::updateTransparency(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateSuperimposing(const RawFrame* /*videoSource*/,
                                        EmuTime::param /*time*/) {
}

void DummyRenderer::updateForegroundColor(uint8_t /*color*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBackgroundColor(uint8_t /*color*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkForegroundColor(uint8_t /*color*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkBackgroundColor(uint8_t /*color*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateBlinkState(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updatePalette(unsigned /*index*/, int /*grb*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateVerticalScroll(int /*scroll*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateHorizontalScrollLow(uint8_t /*scroll*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateHorizontalScrollHigh(uint8_t /*scroll*/, EmuTime::param /*time*/) {
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

void DummyRenderer::updateNameBase(unsigned /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updatePatternBase(unsigned /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateColorBase(unsigned /*addr*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateSpritesEnabled(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateVRAM(unsigned /*offset*/, EmuTime::param /*time*/) {
}

void DummyRenderer::updateWindow(bool /*enabled*/, EmuTime::param /*time*/) {
}

void DummyRenderer::paint(OutputSurface& /*output*/) {
}

} // namespace openmsx
