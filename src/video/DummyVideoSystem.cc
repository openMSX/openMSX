// $Id$

#include "DummyVideoSystem.hh"
#include "components.hh"
#include <cassert>

namespace openmsx {

Rasterizer* DummyVideoSystem::createRasterizer(VDP& /*vdp*/)
{
	assert(false);
	return NULL;
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990& /*vdp*/)
{
	assert(false);
	return NULL;
}

#if COMPONENT_LASERDISC
LDRasterizer* DummyVideoSystem::createLDRasterizer(LaserdiscPlayer& /*ld*/)
{
	assert(false);
	return NULL;
}
#endif

void DummyVideoSystem::flush()
{
}

OutputSurface* DummyVideoSystem::getOutputSurface()
{
	return NULL;
}

} // namespace openmsx
