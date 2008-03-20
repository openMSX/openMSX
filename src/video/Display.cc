// $Id$

#include "Display.hh"
#include "Layer.hh"
#include "VideoSystem.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "FileOperations.hh"
#include "Alarm.hh"
#include "Command.hh"
#include "GlobalCommandController.hh"
#include "InfoTopic.hh"
#include "OSDGUI.hh"
#include "GlobalCliComm.hh"
#include "Timer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "VideoSourceSetting.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MachineConfig.hh"
#include "XMLElement.hh"
#include "VideoSystemChangeListener.hh"
#include "CommandException.hh"
#include "Version.hh"
#include "build-info.hh"
#include "checked_cast.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

using std::string;
using std::vector;

namespace openmsx {

class RepaintAlarm : public Alarm
{
public:
	explicit RepaintAlarm(EventDistributor& eventDistributor);
	virtual bool alarm();
private:
	EventDistributor& eventDistributor;
};

class ScreenShotCmd : public SimpleCommand
{
public:
	ScreenShotCmd(CommandController& commandController, Display& display);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	Display& display;
};

class FpsInfoTopic : public InfoTopic
{
public:
	FpsInfoTopic(InfoCommand& openMSXInfoCommand, Display& display);
	virtual void execute(const vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	Display& display;
};


Display::Display(Reactor& reactor_)
	: alarm(new RepaintAlarm(reactor_.getEventDistributor()))
	, screenShotCmd(new ScreenShotCmd(
		reactor_.getCommandController(), *this))
	, fpsInfo(new FpsInfoTopic(
	      reactor_.getGlobalCommandController().getOpenMSXInfoCommand(), *this))
	, osdGui(new OSDGUI(reactor_.getGlobalCommandController(), *this))
	, reactor(reactor_)
	, renderSettings(new RenderSettings(reactor.getCommandController()))
	, currentRenderer(RendererFactory::UNINITIALIZED)
	, switchInProgress(false)
{
	frameDurationSum = 0;
	for (unsigned i = 0; i < NUM_FRAME_DURATIONS; ++i) {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	}
	prevTimeStamp = Timer::getTime();

	EventDistributor& eventDistributor = reactor.getEventDistributor();
	eventDistributor.registerEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_MACHINE_LOADED_EVENT,
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

	EventDistributor& eventDistributor = reactor.getEventDistributor();
	eventDistributor.unregisterEventListener(OPENMSX_MACHINE_LOADED_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this);

	resetVideoSystem();

	alarm->cancel();
	assert(listeners.empty());
}

void Display::createVideoSystem()
{
	assert(!videoSystem.get());
	assert(currentRenderer == RendererFactory::UNINITIALIZED);
	assert(!switchInProgress);
	currentRenderer = renderSettings->getRenderer().getValue();
	switchInProgress = true;
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

Reactor& Display::getReactor()
{
	return reactor;
}

RenderSettings& Display::getRenderSettings()
{
	return *renderSettings;
}

OSDGUI& Display::getOSDGUI()
{
	return *osdGui;
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

Layer* Display::findLayer(const string& name) const
{
	for (Layers::const_iterator it = layers.begin();
	     it != layers.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return 0;
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

bool Display::signalEvent(shared_ptr<const Event> event)
{
	if (event->getType() == OPENMSX_FINISH_FRAME_EVENT) {
		const FinishFrameEvent& ffe =
			checked_cast<const FinishFrameEvent&>(*event);
		if (!ffe.isSkipped() &&
		    (renderSettings->getVideoSource().getValue() == ffe.getSource())) {
			repaint();
			reactor.getEventDistributor().distributeEvent(
				new SimpleEvent<OPENMSX_FRAME_DRAWN_EVENT>());
		}
	} else if (event->getType() == OPENMSX_DELAYED_REPAINT_EVENT) {
		repaint();
	} else if (event->getType() == OPENMSX_SWITCH_RENDERER_EVENT) {
		doRendererSwitch();
	} else if (event->getType() == OPENMSX_MACHINE_LOADED_EVENT) {
		setWindowTitle();
	}
	return true;
}

void Display::setWindowTitle()
{
	string title = Version::FULL_VERSION;
	if (!Version::RELEASE) {
		title += " [" + BUILD_FLAVOUR + "]";
	}
	if (MSXMotherBoard* motherboard = reactor.getMotherBoard()) {
		if (const MachineConfig* machine = motherboard->getMachineConfig()) {
			const XMLElement& config = machine->getConfig();
			title += " - " +
			    config.getChild("info").getChildData("manufacturer") + " " +
			    config.getChild("info").getChildData("code");
		}
	}
	videoSystem->setWindowTitle(title);
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
	if (switchInProgress) {
		// Part of switching renderer is destroying the current
		// Renderer. A Renderer inherits (indirectly) from VideoLayer.
		// A VideoLayer activates a videosource (MSX, GFX9000). When
		// a videosource is deactiavted we should possibly also switch
		// renderer (TODO is this still true, maybe for SDLGL it is?)
		// and thus this method gets called again. To break the cycle
		// we check for pending switches.
		return;
	}
	RendererFactory::RendererID newRenderer =
		renderSettings->getRenderer().getValue();
	if ((newRenderer != currentRenderer) ||
	    !getVideoSystem().checkSettings()) {
		currentRenderer = newRenderer;
		// don't do the actualing swithing in the TCL callback
		// it seems creating and destroying Settings (= TCL vars)
		// causes problems???
		switchInProgress = true;
		reactor.getEventDistributor().distributeEvent(
			new SimpleEvent<OPENMSX_SWITCH_RENDERER_EVENT>());
	}
}

void Display::doRendererSwitch()
{
	assert(switchInProgress);

	try {
		doRendererSwitch2();
	} catch (MSXException& e) {
		reactor.getGlobalCliComm().printWarning(
			"Couldn't activate renderer " +
			renderSettings->getRenderer().getValueString() +
			": " + e.getMessage());
		renderSettings->getRenderer().changeValue(RendererFactory::SDL);
		currentRenderer = RendererFactory::SDL;
		doRendererSwitch2();
	}

	switchInProgress = false;
}

void Display::doRendererSwitch2()
{
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->preVideoSystemChange();
	}

	resetVideoSystem();
	videoSystem.reset(RendererFactory::createVideoSystem(reactor));
	setWindowTitle();

	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->postVideoSystemChange();
	}
}

void Display::repaint()
{
	if (switchInProgress) {
		// The checkRendererSwitch() method will queue a
		// SWITCH_RENDERER_EVENT, but before that event is handled
		// we shouldn't do any repaints (with inconsistent setting
		// values and render objects). This can happen when this
		// method gets called because of a DELAYED_REPAINT_EVENT
		// (DELAYED_REPAINT_EVENT was already queued before
		// SWITCH_RENDERER_EVENT is queued).
		return;
	}

	alarm->cancel(); // cancel delayed repaint

	assert(videoSystem.get());
	videoSystem->prepare();

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
	Layers::iterator it = std::find(layers.begin(), layers.end(), &layer);
	assert(it != layers.end());
	layers.erase(it);
}

void Display::updateZ(Layer& layer)
{
	// Remove at old Z-index...
	removeLayer(layer);
	// ...and re-insert at new Z-index.
	addLayer(layer);
}


// RepaintAlarm

RepaintAlarm::RepaintAlarm(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
}

bool RepaintAlarm::alarm()
{
	// Note: runs is seperate thread, use event mechanism to repaint
	//       in main thread
	eventDistributor.distributeEvent(
		new SimpleEvent<OPENMSX_DELAYED_REPAINT_EVENT>());
	return false; // don't reschedule
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

FpsInfoTopic::FpsInfoTopic(InfoCommand& openMSXInfoCommand,
                           Display& display_)
	: InfoTopic(openMSXInfoCommand, "fps")
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

