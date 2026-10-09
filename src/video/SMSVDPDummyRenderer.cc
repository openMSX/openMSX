#include "SMSVDPDummyRenderer.hh"

namespace openmsx {

PostProcessor* SMSVDPDummyRenderer::getPostProcessor() const
{
	return nullptr;
}

bool SMSVDPDummyRenderer::isActive() const
{
	return false;
}

void SMSVDPDummyRenderer::frameEnd(EmuTime /*time*/)
{
}

void SMSVDPDummyRenderer::drawLine(unsigned /*y*/, std::span<const uint8_t, 320> /*paletteIndices*/)
{
}

} // namespace openmsx
