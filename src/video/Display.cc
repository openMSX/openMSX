// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "InfoCommand.hh"
#include "CliCommOutput.hh"
#include "Scheduler.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

// Display:

std::auto_ptr<Display> Display::INSTANCE;

Display::Display(std::auto_ptr<VideoSystem> videoSystem_)
	: videoSystem(videoSystem_)
	, screenShotCmd(*this)
	, fpsInfo(*this)
{
	frameDurationSum = 0;
	for (unsigned i = 0; i < NUM_FRAME_DURATIONS; ++i) {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	}
	prevTimeStamp = Timer::getTime();

	EventDistributor::instance().registerEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().registerEventListener(
		DELAYED_REPAINT_EVENT, *this, EventDistributor::DETACHED);
	CommandController::instance().registerCommand(
		&screenShotCmd, "screenshot" );
	InfoCommand::instance().registerTopic("fps", &fpsInfo);
}

Display::~Display()
{
	InfoCommand::instance().unregisterTopic("fps", &fpsInfo);
	CommandController::instance().unregisterCommand(
		&screenShotCmd, "screenshot" );
	EventDistributor::instance().unregisterEventListener(
		DELAYED_REPAINT_EVENT, *this, EventDistributor::DETACHED);
	EventDistributor::instance().unregisterEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE);
	// Prevent callbacks first...
	for (Layers::iterator it = layers.begin(); it != layers.end(); ++it) {
		(*it)->display = NULL;
	}
	// ...then destruct layers.
	for (Layers::iterator it = layers.begin(); it != layers.end(); ++it) {
		delete *it;
	}
	
	alarm.cancel();
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
	if (event.getType() == FINISH_FRAME_EVENT) {
		const FinishFrameEvent& ffe = static_cast<const FinishFrameEvent&>(event);
		VideoSource eventSource = ffe.getSource();
		VideoSource visibleSource = 
			RenderSettings::instance().getVideoSource()->getValue();

		bool draw = visibleSource == eventSource;
		if (draw) {
			repaint();
		}

		RealTime::instance().sync(Scheduler::instance().getCurrentTime(), draw);
	} else if (event.getType() == DELAYED_REPAINT_EVENT) {
		repaint();
	} else {
		assert(false);
	}
	return true;
}

void Display::repaint()
{
	alarm.cancel(); // cancel delayed repaint

	assert(videoSystem.get());
	// TODO: Is this the proper way to react?
	//       Behind this abstraction is SDL_LockSurface,
	//       which is severely underdocumented:
	//       it is unknown whether a failure is transient or permanent.
	if (!videoSystem->prepare()) return;
	
	for(Layers::iterator it = baseLayer(); it != layers.end(); ++it) {
		if ((*it)->coverage != Layer::COVER_NONE) {
			//std::cout << "Painting layer " << (*it)->getName() << std::endl;
			(*it)->paint();
		}
	}
	
	videoSystem->flush();

	// update fps statistics
	unsigned long long now = Timer::getTime();
	unsigned long long duration = now - prevTimeStamp;
	prevTimeStamp = now;
	frameDurationSum += duration - frameDurations.removeBack();
	frameDurations.addFront(duration);
}

void Display::repaintDelayed(unsigned long long delta)
{
	if (alarm.pending()) {
		// already a pending repaint
		return;
	}
	alarm.schedule(delta);
}

void Display::addLayer(Layer* layer)
{
	const int z = layer->z;
	Layers::iterator it;
	for(it = layers.begin(); it != layers.end() && (*it)->z < z; ++it)
		/* do nothing */;
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


// RepaintAlarm inner class

void Display::RepaintAlarm::alarm()
{
	// Note: runs is seperate thread, use event mechanism to repaint
	//       in main thread
	EventDistributor::instance().distributeEvent(
		new SimpleEvent<DELAYED_REPAINT_EVENT>());
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

// FpsInfoTopic inner class:

Display::FpsInfoTopic::FpsInfoTopic(Display& parent_)
	: parent(parent_)
{
}

void Display::FpsInfoTopic::execute(const vector<CommandArgument>& /*tokens*/,
                                    CommandArgument& result) const
{
	double fps = 1000000.0 * NUM_FRAME_DURATIONS / parent.frameDurationSum;
	result.setDouble(fps);
}

string Display::FpsInfoTopic::help (const vector<string>& /*tokens*/) const
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx

