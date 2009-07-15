// $Id$

#include "DummyVideoSystem.hh"
#include "DummyRasterizer.hh"
#include "V9990DummyRasterizer.hh"
#include "components.hh"

#ifdef COMPONENT_LASERDISC
#include "LDDummyRasterizer.hh"
#endif

namespace openmsx {

Rasterizer* DummyVideoSystem::createRasterizer(VDP& /*vdp*/)
{
	return new DummyRasterizer();
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990& /*vdp*/)
{
	return new V9990DummyRasterizer();
}

#ifdef COMPONENT_LASERDISC
LDRasterizer* DummyVideoSystem::createLDRasterizer(LaserdiscPlayer& /*ld*/)
{
	return new LDDummyRasterizer();
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
