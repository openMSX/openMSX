// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "CliCommOutput.hh"
#include "Scheduler.hh"
#include "RealTime.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// Layer:

Layer::Layer(Coverage coverage, ZIndex z)
{
	display = NULL;
	this->coverage = coverage;
	this->z = z;
}

Layer::~Layer()
{
}

void Layer::setCoverage(Coverage coverage)
{
	this->coverage = coverage;
	if (display) display->updateCoverage(this, coverage);
}

void Layer::setZ(ZIndex z)
{
	this->z = z;
	if (display) display->updateZ(this, z);
}

// Display:

std::auto_ptr<Display> Display::INSTANCE;

Display::Display(std::auto_ptr<VideoSystem> videoSystem)
	: screenShotCmd(*this)
{
	this->videoSystem = videoSystem;

	EventDistributor::instance().registerEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE );
	CommandController::instance().registerCommand(
		&screenShotCmd, "screenshot" );
}

Display::~Display()
{
	CommandController::instance().unregisterCommand(
		&screenShotCmd, "screenshot" );
	EventDistributor::instance().unregisterEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE );
	// Prevent callbacks first...
	for (Layers::iterator it = layers.begin();
		it != layers.end(); ++it
	) {
		(*it)->display = NULL;
	}
	// ...then destruct layers.
	for (Layers::iterator it = layers.begin();
		it != layers.end(); ++it
	) {
		delete *it;
	}
}

Display::Layers::iterator Display::baseLayer()
{
	// Note: It is possible to cache this, but since the number of layers is
	//       low at the moment, it's not really worth it.
	Layers::iterator it = layers.end();
	while (true) {
		if (it == layers.begin()) {
			// There should always be at least one opaque layer.
			// TODO: This is not true for DummyVideoSystem.
			//       Anyway, a missing layer will probably stand out visually,
			//       so do we really have to assert on it?
			//assert(false);
			return it;
		}
		--it;
		if ((*it)->coverage == Layer::COVER_FULL) return it;
	}
}

bool Display::signalEvent(const Event& event)
{
	assert(event.getType() == FINISH_FRAME_EVENT);
	
	const FinishFrameEvent& ffe = static_cast<const FinishFrameEvent&>(event);
	RenderSettings::VideoSource eventSource = ffe.getSource();
	RenderSettings::VideoSource visibleSource = 
		RenderSettings::instance().getVideoSource()->getValue();

	bool draw = visibleSource == eventSource;
	if(draw) {
		repaint();
	}

	RealTime::instance().sync(Scheduler::instance().getCurrentTime(),
			                  draw);
	return true;
}

void Display::repaint()
{
	assert(videoSystem.get());
	// TODO: Is this the proper way to react?
	//       Behind this abstraction is SDL_LockSurface,
	//       which is severely underdocumented:
	//       it is unknown whether a failure is transient or permanent.
	if (!videoSystem->prepare()) return;
	
	Layers::iterator it = baseLayer();
	for (; it != layers.end(); ++it) {
		if ((*it)->coverage != Layer::COVER_NONE) {
			// Paint this layer.
			//cerr << "Painting layer " << (*it)->layer->getName() << "\n";
			(*it)->paint();
		}
	}
	
	videoSystem->flush();
}

void Display::addLayer(Layer* layer)
{
	const int z = layer->z;
	Layers::iterator it;
	for (it = layers.begin(); it != layers.end() && (*it)->z < z; ++it);
	layers.insert(it, layer);
	layer->display = this;
}

void Display::updateCoverage(Layer* layer, Layer::Coverage coverage)
{
	// Do nothing.
}

void Display::updateZ(Layer* layer, Layer::ZIndex z)
{
	// Remove at old Z-index...
	layers.erase(std::find(layers.begin(), layers.end(), layer));
	// ...and re-insert at new Z-index.
	addLayer(layer);
}

// ScreenShotCmd inner class:

Display::ScreenShotCmd::ScreenShotCmd(Display& display_)
	: display(display_)
{
}

string Display::ScreenShotCmd::execute(const vector<string>& tokens)
{
	string filename;
	switch (tokens.size()) {
	case 1:
		filename = ScreenShotSaver::getFileName();
		break;
	case 2:
		filename = tokens[1];
		break;
	default:
		throw SyntaxError();
	}
	
	Display::INSTANCE->getVideoSystem()->takeScreenShot(filename);
	CliCommOutput::instance().printInfo("Screen saved to " + filename);
	return filename;
}

string Display::ScreenShotCmd::help(const vector<string>& /*tokens*/) const
{
	return
		"screenshot             Write screenshot to file \"openmsxNNNN.png\"\n"
		"screenshot <filename>  Write screenshot to indicated file\n";
}

} // namespace openmsx

