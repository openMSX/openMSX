#include "V9990DummyRenderer.hh"

namespace openmsx {

PostProcessor* V9990DummyRenderer::getPostProcessor() const
{
	return nullptr;
}

void V9990DummyRenderer::reset(EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::frameStart(EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::frameEnd(EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::renderUntil(EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::updateDisplayEnabled(bool /*enabled*/,
                                              EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::setDisplayMode(V9990DisplayMode /*mode*/,
	                                EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::setColorMode(V9990ColorMode /*mode*/,
	                              EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::updatePalette(
	int /*index*/, uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/, bool /*ys*/,
	EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::updateSuperimposing(
	bool /*enabled*/, EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::updateBackgroundColor(
	int /*index*/, EmuTime::param /*time*/)
{
}

void V9990DummyRenderer::updateScrollAX(EmuTime::param /*time*/)
{
}
void V9990DummyRenderer::updateScrollBX(EmuTime::param /*time*/)
{
}
void V9990DummyRenderer::updateScrollAYLow(EmuTime::param /*time*/)
{
}
void V9990DummyRenderer::updateScrollBYLow(EmuTime::param /*time*/)
{
}

} // namespace openmsx
