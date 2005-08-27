// $Id$

#include "DummyVideoSystem.hh"
#include "V9990DummyRasterizer.hh"
#include "Display.hh"

namespace openmsx {

DummyVideoSystem::DummyVideoSystem(Display& display)
{
	display.resetVideoSystem(); // destruct old layers
	display.setVideoSystem(this);
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

