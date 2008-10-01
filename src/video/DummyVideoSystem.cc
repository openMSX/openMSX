// $Id$

#include "DummyVideoSystem.hh"
#include "DummyRasterizer.hh"
#include "V9990DummyRasterizer.hh"

namespace openmsx {

Rasterizer* DummyVideoSystem::createRasterizer(VDP& /*vdp*/)
{
	return new DummyRasterizer();
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990& /*vdp*/)
{
	return new V9990DummyRasterizer();
}

void DummyVideoSystem::flush()
{
}

OutputSurface* DummyVideoSystem::getOutputSurface()
{
	return NULL;
}

} // namespace openmsx
