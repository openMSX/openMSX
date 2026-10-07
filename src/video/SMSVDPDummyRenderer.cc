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

void SMSVDPDummyRenderer::reset(EmuTime /*time*/)
{
}

void SMSVDPDummyRenderer::frameStart(EmuTime /*time*/)
{
}

void SMSVDPDummyRenderer::frameEnd(EmuTime /*time*/)
{
}

void SMSVDPDummyRenderer::drawLine(unsigned /*y*/, std::span<const uint32_t, 320> /*pixels*/)
{
}

} // namespace openmsx
