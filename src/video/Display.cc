// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "CliCommOutput.hh"
#include <cassert>


namespace openmsx {

// Layer:

Layer::~Layer()
{
}

// Display:

auto_ptr<Display> Display::INSTANCE;

Display::Display(auto_ptr<VideoSystem> videoSystem)
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
	for (vector<LayerInfo*>::iterator it = layers.begin();
		it != layers.end(); ++it
	) {
		delete (*it)->layer;
		delete *it;
	}
}

vector<Display::LayerInfo*>::iterator Display::baseLayer()
{
	vector<LayerInfo*>::iterator it = layers.end();
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
		if ((*it)->coverage == COVER_FULL) return it;
	}
}

bool Display::signalEvent(const Event& /*event*/)
{
	repaint();
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
	
	vector<LayerInfo*>::iterator it = baseLayer();
	for (; it != layers.end(); ++it) {
		if ((*it)->coverage != COVER_NONE) {
			assert(isActive((*it)->layer));
			// Paint this layer.
			//cerr << "Painting layer " << (*it)->layer->getName() << "\n";
			(*it)->layer->paint();
		}
	}
	
	videoSystem->flush();
}

void Display::addLayer(Layer* layer, int z)
{
	addLayer(new LayerInfo(layer, z));
}

void Display::addLayer(LayerInfo* info)
{
	vector<LayerInfo*>::iterator it;
	for (it = layers.begin(); it != layers.end() && (*it)->z < info->z; ++it);
	layers.insert(it, info);
}

vector<Display::LayerInfo*>::iterator Display::findLayer(Layer* layer)
{
	for (vector<LayerInfo*>::iterator it = layers.begin(); ; ++it) {
		assert(it != layers.end());
		if ((*it)->layer == layer) {
			return it;
		}
	}
}

bool Display::isActive(Layer* layer)
{
	vector<LayerInfo*>::iterator it = baseLayer();
	for (; it != layers.end(); ++it) {
		if ((*it)->layer == layer) {
			// Layer in active part of stack.
			return (*it)->coverage != COVER_NONE;
		}
	}
	// Layer not in active part of stack.
	return false;
}

void Display::setZ(Layer* layer, int z)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	LayerInfo* info = *it;
	info->z = z;
	layers.erase(it);
	addLayer(info);
}

void Display::setCoverage(Layer* layer, Coverage coverage)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	(*it)->coverage = coverage;
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

// LayerInfo:

Display::LayerInfo::LayerInfo(Layer* layer, int z)
{
	this->layer = layer;
	this->z = z;
	coverage = COVER_NONE;
}

} // namespace openmsx

