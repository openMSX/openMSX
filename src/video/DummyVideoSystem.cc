// $Id$

#include "DummyVideoSystem.hh"
#include "V9990DummyRasterizer.hh"
#include "Display.hh"

namespace openmsx {

DummyVideoSystem::DummyVideoSystem()
{
	// Destruct old layers.
	Display::INSTANCE.reset();
	Display* display = new Display(std::auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
}

DummyVideoSystem::~DummyVideoSystem()
{
}

V9990Rasterizer* DummyVideoSystem::createV9990Rasterizer(V9990* vdp)
{
	return new V9990DummyRasterizer();
}

void DummyVideoSystem::flush()
{
}

} // namespace openmsx

