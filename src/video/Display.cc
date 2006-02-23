// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "FileOperations.hh"
#include "Alarm.hh"
#include "CommandConsole.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "CliComm.hh"
#include "Scheduler.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "VideoSourceSetting.hh"
#include "MSXMotherBoard.hh"
#include "MachineConfig.hh"
#include "VideoSystemChangeListener.hh"
#include "Version.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

// Needed for workaround for "black flashes" problem in SDLGL renderer:
#include "SDLGLVideoSystem.hh"
#include "components.hh"

using std::string;
using std::vector;

namespace openmsx {

class RepaintAlarm : public Alarm
{
public:
	RepaintAlarm(EventDistributor& eventDistributor);
	virtual void alarm();
private:
	EventDistributor& eventDistributor;
};

class ScreenShotCmd : public SimpleCommand
{
public:
	ScreenShotCmd(CommandController& commandController, Display& display);
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	Display& display;
};

class FpsInfoTopic : public InfoTopic
{
public:
	FpsInfoTopic(CommandController& commandController, Display& display);
	virtual void execute(const std::vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	Display& display;
};


Display::Display(MSXMotherBoard& motherboard_)
	: currentRenderer(RendererFactory::UNINITIALIZED)
	, alarm(new RepaintAlarm(motherboard_.getEventDistributor()))
	, screenShotCmd(new ScreenShotCmd(
		motherboard_.getCommandController(), *this))
	, fpsInfo(new FpsInfoTopic(motherboard_.getCommandController(), *this))
	, motherboard(motherboard_)
	, renderSettings(new RenderSettings(motherboard.getCommandController()))
	, switchInProgress(0)
{
	// TODO clean up
	motherboard.getCommandConsole().setDisplay(this);

	frameDurationSum = 0;
	for (unsigned i = 0; i < NUM_FRAME_DURATIONS; ++i) {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	}
	prevTimeStamp = Timer::getTime();

	EventDistributor& eventDistributor = motherboard.getEventDistributor();
	eventDistributor.registerEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);

	renderSettings->getRenderer().attach(*this);
	renderSettings->getFullScreen().attach(*this);
	renderSettings->getScaleFactor().attach(*this);
	renderSettings->getVideoSource().attach(*this);
}

Display::~Display()
{
	renderSettings->getRenderer().detach(*this);
	renderSettings->getFullScreen().detach(*this);
	renderSettings->getScaleFactor().detach(*this);
	renderSettings->getVideoSource().detach(*this);

	EventDistributor& eventDistributor = motherboard.getEventDistributor();
	eventDistributor.unregisterEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this);

	resetVideoSystem();

	alarm->cancel();
	motherboard.getCommandConsole().setDisplay(0);
	assert(listeners.empty());
}

void Display::createVideoSystem()
{
	assert(!videoSystem.get());
	assert(currentRenderer == RendererFactory::UNINITIALIZED);
	currentRenderer = renderSettings->getRenderer().getValue();
	assert(!switchInProgress);
	++switchInProgress;
	doRendererSwitch();
}

VideoSystem& Display::getVideoSystem()
{
	assert(videoSystem.get());
	return *videoSystem;
}

void Display::resetVideoSystem()
{
	videoSystem.reset();
	for (Layers::const_iterator it = layers.begin();
	     it != layers.end(); ++it) {
		std::cerr << (*it)->getName() << std::endl;
	}
	assert(layers.empty());
}

RenderSettings& Display::getRenderSettings()
{
	return *renderSettings;
}

void Display::attach(VideoSystemChangeListener& listener)
{
	assert(count(listeners.begin(), listeners.end(), &listener) == 0);
	listeners.push_back(&listener);
}

void Display::detach(VideoSystemChangeListener& listener)
{
	Listeners::iterator it =
		find(listeners.begin(), listeners.end(), &listener);
	assert(it != listeners.end());
	listeners.erase(it);
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

void Display::signalEvent(const Event& event)
{
	if (event.getType() == OPENMSX_FINISH_FRAME_EVENT) {
		const FinishFrameEvent& ffe = static_cast<const FinishFrameEvent&>(event);
		if (!ffe.isSkipped() &&
		    (renderSettings->getVideoSource().getValue() == ffe.getSource())) {
			repaint();
			motherboard.getEventDistributor().distributeEvent(
				new SimpleEvent<OPENMSX_FRAME_DRAWN_EVENT>());
		}
	} else if (event.getType() == OPENMSX_DELAYED_REPAINT_EVENT) {
		repaint();
	} else if (event.getType() == OPENMSX_SWITCH_RENDERER_EVENT) {
		doRendererSwitch();
	} else {
		assert(false);
	}
}

void Display::update(const Setting& setting)
{
	if (&setting == &renderSettings->getRenderer()) {
		checkRendererSwitch();
	} else if (&setting == &renderSettings->getFullScreen()) {
		checkRendererSwitch();
	} else if (&setting == &renderSettings->getScaleFactor()) {
		checkRendererSwitch();
	} else if (&setting == &renderSettings->getVideoSource()) {
		checkRendererSwitch();
	} else {
		assert(false);
	}
}

void Display::checkRendererSwitch()
{
	// Tell renderer to sync with render settings.
	RendererFactory::RendererID newRenderer =
		renderSettings->getRenderer().getValue();
	if ((newRenderer != currentRenderer) ||
	    !getVideoSystem().checkSettings()) {
		currentRenderer = newRenderer;
		// don't do the actualing swithing in the TCL callback
		// it seems creating and destroying Settings (= TCL vars)
		// causes problems???
		if (switchInProgress) return;
		++switchInProgress;
		motherboard.getEventDistributor().distributeEvent(
		      new SimpleEvent<OPENMSX_SWITCH_RENDERER_EVENT>());
	}
}

void Display::doRendererSwitch()
{
	assert(switchInProgress);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->preVideoSystemChange();
	}

	resetVideoSystem();
	videoSystem.reset(RendererFactory::createVideoSystem(motherboard));
	const XMLElement& config = motherboard.getMachineConfig().getConfig();
	std::string title = Version::FULL_VERSION + " - " +
		config.getChild("info").getChildData("manufacturer") + " " +
		config.getChild("info").getChildData("code");
	videoSystem->setWindowTitle(title);

	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->postVideoSystemChange();
	}
	--switchInProgress;
}

void Display::repaint()
{
	alarm->cancel(); // cancel delayed repaint

	assert(videoSystem.get());
	// TODO: Is this the proper way to react?
	//       Behind this abstraction is SDL_LockSurface,
	//       which is severely underdocumented:
	//       it is unknown whether a failure is transient or permanent.
	if (!videoSystem->prepare()) return;

	for (Layers::iterator it = baseLayer(); it != layers.end(); ++it) {
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
	if (alarm->pending()) {
		// already a pending repaint
		return;
	}

#ifdef COMPONENT_GL
	if (dynamic_cast<SDLGLVideoSystem*>(videoSystem.get())) {
		// Ugly workaround: on GL limit the frame rate of the delayed
		// repaints. Because of a limitation in the SDL GL environment
		// we cannot render to texture (or aux buffer) and indirectly
		// this causes black flashes when a delayed repaint comes at
		// the wrong moment.
		// By limiting the frame rate we hope the delayed repaint only
		// get triggered during pause, where it's ok (is it?)
		if (delta < 200000) delta = 200000; // at most 8fps
	}
#endif

	alarm->schedule(delta);
}

void Display::addLayer(Layer& layer)
{
	int z = layer.getZ();
	Layers::iterator it;
	for (it = layers.begin(); it != layers.end() && (*it)->getZ() < z; ++it)
		/* do nothing */;
	layer.display = this;
	layers.insert(it, &layer);
}

void Display::removeLayer(Layer& layer)
{
	Layers::iterator it = find(layers.begin(), layers.end(), &layer);
	assert(it != layers.end());
	layers.erase(it);
}

void Display::updateZ(Layer& layer, Layer::ZIndex /*z*/)
{
	// Remove at old Z-index...
	Layers::iterator it = std::find(layers.begin(), layers.end(), &layer);
	assert(it != layers.end());
	layers.erase(it);

	// ...and re-insert at new Z-index.
	addLayer(layer);
}


// RepaintAlarm

RepaintAlarm::RepaintAlarm(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
}

void RepaintAlarm::alarm()
{
	// Note: runs is seperate thread, use event mechanism to repaint
	//       in main thread
	eventDistributor.distributeEvent(
		new SimpleEvent<OPENMSX_DELAYED_REPAINT_EVENT>());
}


// ScreenShotCmd

ScreenShotCmd::ScreenShotCmd(CommandController& commandController,
                                      Display& display_)
	: SimpleCommand(commandController, "screenshot")
	, display(display_)
{
}

string ScreenShotCmd::execute(const vector<string>& tokens)
{
	string filename;
	switch (tokens.size()) {
	case 1:
		filename = FileOperations::getNextNumberedFileName("screenshots", "openmsx", ".png");
		break;
	case 2:
		if (tokens[1] == "-prefix") {
			throw SyntaxError();
		} else {
			filename = tokens[1];
		}
		break;
	case 3:
		if (tokens[1] == "-prefix") {
			filename = FileOperations::getNextNumberedFileName("screenshots", tokens[2], ".png");
		} else {
			throw SyntaxError();
		}
		break;
	default:
		throw SyntaxError();
	}

	display.getVideoSystem().takeScreenShot(filename);
	getCommandController().getCliComm().printInfo("Screen saved to " + filename);
	return filename;
}

string ScreenShotCmd::help(const vector<string>& /*tokens*/) const
{
	return
		"screenshot              Write screenshot to file \"openmsxNNNN.png\"\n"
		"screenshot <filename>   Write screenshot to indicated file\n"
		"screenshot -prefix foo  Write screenshot to file \"fooNNNN.png\"\n";
}

// FpsInfoTopic

FpsInfoTopic::FpsInfoTopic(CommandController& commandController,
                           Display& display_)
	: InfoTopic(commandController, "fps")
	, display(display_)
{
}

void FpsInfoTopic::execute(const vector<TclObject*>& /*tokens*/,
                           TclObject& result) const
{
	double fps = 1000000.0 * Display::NUM_FRAME_DURATIONS / display.frameDurationSum;
	result.setDouble(fps);
}

string FpsInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx

