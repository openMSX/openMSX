// $Id$

#include "DummyVideoSystem.hh"
#include "V9990DummyRasterizer.hh"

namespace openmsx {

DummyVideoSystem::DummyVideoSystem()
{
}

DummyVideoSystem::~DummyVideoSystem()
{
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990& /*vdp*/)
{
	return new V9990DummyRasterizer();
}

void DummyVideoSystem::flush()
{
}

} // namespace openmsx

