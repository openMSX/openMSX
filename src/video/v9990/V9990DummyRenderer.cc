#include "V9990DummyRenderer.hh"

namespace openmsx {

PostProcessor* V9990DummyRenderer::getPostProcessor() const
{
	return nullptr;
}

void V9990DummyRenderer::reset(EmuTime /*time*/)
{
}

void V9990DummyRenderer::frameStart(EmuTime /*time*/)
{
}

void V9990DummyRenderer::frameEnd(EmuTime /*time*/)
{
}

void V9990DummyRenderer::renderUntil(EmuTime /*time*/)
{
}

void V9990DummyRenderer::updateDisplayEnabled(bool /*enabled*/,
                                              EmuTime /*time*/)
{
}

void V9990DummyRenderer::setDisplayMode(V9990DisplayMode /*mode*/,
	                                EmuTime /*time*/)
{
}

void V9990DummyRenderer::setColorMode(V9990ColorMode /*mode*/,
	                              EmuTime /*time*/)
{
}

void V9990DummyRenderer::updatePalette(
	int /*index*/, uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/, bool /*ys*/,
	EmuTime /*time*/)
{
}

void V9990DummyRenderer::updateSuperimposing(
	bool /*enabled*/, EmuTime /*time*/)
{
}

void V9990DummyRenderer::updateBackgroundColor(
	int /*index*/, EmuTime /*time*/)
{
}

void V9990DummyRenderer::updateScrollAX(EmuTime /*time*/)
{
}
void V9990DummyRenderer::updateScrollBX(EmuTime /*time*/)
{
}
void V9990DummyRenderer::updateScrollAYLow(EmuTime /*time*/)
{
}
void V9990DummyRenderer::updateScrollBYLow(EmuTime /*time*/)
{
}

} // namespace openmsx
