// $Id$

#include "DummyVideoSystem.hh"
#include "DummyRenderer.hh"
#include "Display.hh"
#include "RendererFactory.hh"


namespace openmsx {

DummyVideoSystem::DummyVideoSystem()
{
	// Destruct old layers.
	Display::INSTANCE.reset();
	Display* display = new Display(auto_ptr<VideoSystem>(this));
	Renderer* renderer = new DummyRenderer(RendererFactory::DUMMY);
	display->addLayer(dynamic_cast<Layer*>(renderer));
	display->setAlpha(dynamic_cast<Layer*>(renderer), 255);
	Display::INSTANCE.reset(display);

	this->renderer = renderer;
}

DummyVideoSystem::~DummyVideoSystem()
{
}

void DummyVideoSystem::flush()
{
}

} // namespace openmsx

