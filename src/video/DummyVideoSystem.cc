// $Id$

#include "DummyVideoSystem.hh"
#include "DummyRenderer.hh"
#include "Display.hh"


namespace openmsx {

DummyVideoSystem::DummyVideoSystem()
{
	// Destruct old layers.
	Display::INSTANCE.reset();
	Display* display = new Display(auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
}

DummyVideoSystem::~DummyVideoSystem()
{
}

void DummyVideoSystem::flush()
{
}

} // namespace openmsx

