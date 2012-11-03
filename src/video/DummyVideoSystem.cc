// $Id$

#include "DummyVideoSystem.hh"
#include "components.hh"
#include "unreachable.hh"

namespace openmsx {

Rasterizer* DummyVideoSystem::createRasterizer(VDP& /*vdp*/)
{
	UNREACHABLE;
	return nullptr;
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990& /*vdp*/)
{
	UNREACHABLE;
	return nullptr;
}

#if COMPONENT_LASERDISC
LDRasterizer* DummyVideoSystem::createLDRasterizer(LaserdiscPlayer& /*ld*/)
{
	UNREACHABLE;
	return nullptr;
}
#endif

void DummyVideoSystem::flush()
{
}

OutputSurface* DummyVideoSystem::getOutputSurface()
{
	return nullptr;
}

} // namespace openmsx
