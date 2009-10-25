// $Id$

#include "Display.hh"
#include "Layer.hh"
#include "VideoSystem.hh"
#include "PostProcessor.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "AlarmEvent.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "OSDGUI.hh"
#include "CliComm.hh"
#include "CommandConsole.hh"
#include "Timer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "VideoSourceSetting.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "VideoSystemChangeListener.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "Version.hh"
#include "build-info.hh"
#include "checked_cast.hh"
#include "unreachable.hh"
#include "openmsx.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

class ScreenShotCmd : public SimpleCommand
{
public:
	ScreenShotCmd(CommandController& commandController, Display& display);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
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
	: alarm(new AlarmEvent(reactor_.getEventDistributor(), *this,
	                       OPENMSX_DELAYED_REPAINT_EVENT))
	, screenShotCmd(new ScreenShotCmd(
		reactor_.getCommandController(), *this))
	, fpsInfo(new FpsInfoTopic(reactor_.getOpenMSXInfoCommand(), *this))
	, osdGui(new OSDGUI(reactor_.getCommandController(), *this))
	, reactor(reactor_)
	, renderSettings(new RenderSettings(reactor.getCommandController()))
	, commandConsole(new CommandConsole(
		reactor.getGlobalCommandController(),
		reactor.getEventDistributor(), *this))
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
	eventDistributor.registerEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_MACHINE_LOADED_EVENT,
			*this);
	eventDistributor.registerEventListener(OPENMSX_EXPOSE_EVENT,
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
	eventDistributor.unregisterEventListener(OPENMSX_EXPOSE_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_MACHINE_LOADED_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_SWITCH_RENDERER_EVENT,
			*this);
	eventDistributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this);

	PRT_DEBUG("Reset video system...");
	resetVideoSystem();
	PRT_DEBUG("Reset video system... DONE!");

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

CliComm& Display::getCliComm() const
{
	return reactor.getCliComm();
}

RenderSettings& Display::getRenderSettings() const
{
	return *renderSettings;
}

OSDGUI& Display::getOSDGUI() const
{
	return *osdGui;
}

CommandConsole& Display::getCommandConsole()
{
	return *commandConsole;
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
			//UNREACHABLE;
			return it;
		}
		--it;
		if ((*it)->getCoverage() == Layer::COVER_FULL) return it;
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
				new SimpleEvent(OPENMSX_FRAME_DRAWN_EVENT));
		}
	} else if (event->getType() == OPENMSX_DELAYED_REPAINT_EVENT) {
		repaint();
	} else if (event->getType() == OPENMSX_SWITCH_RENDERER_EVENT) {
		doRendererSwitch();
	} else if (event->getType() == OPENMSX_MACHINE_LOADED_EVENT) {
		setWindowTitle();
	} else if (event->getType() == OPENMSX_EXPOSE_EVENT) {
		// Don't render too often, and certainly not when the screen
		// will anyway soon be rendered.
		repaintDelayed(100 * 1000); // 10fps
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
		if (const HardwareConfig* machine = motherboard->getMachineConfig()) {
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
		UNREACHABLE;
	}
}

void Display::checkRendererSwitch()
{
	if (switchInProgress) {
		// Part of switching renderer is destroying the current
		// Renderer. A Renderer inherits (indirectly) from VideoLayer.
		// A VideoLayer activates a videosource (MSX, GFX9000). When
		// a videosource is deactiavted we should possibly also switch
		// renderer (TODO is this still true, maybe it was for SDLGL?)
		// and thus this method gets called again. To break the cycle
		// we check for pending switches.
		return;
	}
	RendererFactory::RendererID newRenderer =
		renderSettings->getRenderer().getValue();
	if ((newRenderer != currentRenderer) ||
	    !getVideoSystem().checkSettings()) {
		currentRenderer = newRenderer;
		// don't do the actualing swithing in the Tcl callback
		// it seems creating and destroying Settings (= Tcl vars)
		// causes problems???
		switchInProgress = true;
		reactor.getEventDistributor().distributeEvent(
			new SimpleEvent(OPENMSX_SWITCH_RENDERER_EVENT));
	}
}

void Display::doRendererSwitch()
{
	assert(switchInProgress);

	try {
		doRendererSwitch2();
	} catch (MSXException& e) {
		getCliComm().printWarning(
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
	OutputSurface* surface = videoSystem->getOutputSurface();
	if (surface) {
		repaint(*surface);
		videoSystem->flush();
	}

	// update fps statistics
	unsigned long long now = Timer::getTime();
	unsigned long long duration = now - prevTimeStamp;
	prevTimeStamp = now;
	frameDurationSum += duration - frameDurations.removeBack();
	frameDurations.addFront(duration);
}

void Display::repaint(OutputSurface& surface)
{
	for (Layers::iterator it = baseLayer(); it != layers.end(); ++it) {
		if ((*it)->getCoverage() != Layer::COVER_NONE) {
			(*it)->paint(surface);
		}
	}
}

void Display::repaintDelayed(unsigned long long delta)
{
	if (alarm->pending()) {
		// already a pending repaint
		return;
	}
	alarm->schedule(unsigned(delta));
}

void Display::addLayer(Layer& layer)
{
	int z = layer.getZ();
	Layers::iterator it;
	for (it = layers.begin(); it != layers.end() && (*it)->getZ() < z; ++it) {
		// nothing
	}
	layers.insert(it, &layer);
	layer.setDisplay(*this);
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


// ScreenShotCmd

ScreenShotCmd::ScreenShotCmd(CommandController& commandController,
                             Display& display_)
	: SimpleCommand(commandController, "screenshot")
	, display(display_)
{
}

string ScreenShotCmd::execute(const vector<string>& tokens)
{
	bool rawShot = false;
	bool withOsd = false;
	bool doubleSize = false;
	string prefix = "openmsx";
	vector<string> arguments;
	for (unsigned i = 1; i < tokens.size(); ++i) {
		if (StringOp::startsWith(tokens[i], "-")) {
			if (tokens[i] == "--") {
				arguments.insert(arguments.end(),
					tokens.begin() + i + 1, tokens.end());
				break;
			}
			if (tokens[i] == "-prefix") {
				if (++i == tokens.size()) {
					throw CommandException("Missing argument");
				}
				prefix = tokens[i];
			} else if (tokens[i] == "-raw") {
				rawShot = true;
			} else if (tokens[i] == "-msxonly") {
				display.getCliComm().printWarning(
					"The -msxonly option has been deprecated and will "
					"be removed in a future release. Instead, use the "
					"-raw option for the same effect.");
				rawShot = true;
			} else if (tokens[i] == "-doublesize") {
				doubleSize = true;
			} else if (tokens[i] == "-with-osd") {
				withOsd = true;
			} else {
				throw CommandException("Invalid option: " + tokens[i]);
			}
		} else {
			arguments.push_back(tokens[i]);
		}
	}
	if (doubleSize && !rawShot) {
		throw CommandException("-doublesize option can only be used in "
		                       "combination with -raw");
	}
	if (rawShot && withOsd) {
		throw CommandException("-with-osd cannot be used in "
		                       "combination with -raw");
	}

	string filename;
	switch (arguments.size()) {
	case 0:
		filename = FileOperations::getNextNumberedFileName(
				"screenshots", prefix, ".png");
		break;
	case 1:
		filename = FileOperations::expandTilde(arguments[0]);
		break;
	default:
		throw SyntaxError();
	}

	if (!rawShot) {
		// include all layers (OSD stuff, console)
		display.getVideoSystem().takeScreenShot(filename, withOsd);
	} else {
		VideoSourceSetting& videoSource =
			display.getRenderSettings().getVideoSource();
		Layer* layer = display.findLayer(
			(videoSource.getValue() == VIDEO_MSX) ?
			"V99x8 PostProcessor" : "V9990 PostProcessor");
		PostProcessor* pp = dynamic_cast<PostProcessor*>(layer);
		if (!pp) {
			throw CommandException(
				"Current renderer doesn't support taking screenshots.");
		}
		unsigned height = doubleSize ? 480 : 240;
		pp->takeScreenShot(height, filename);
	}

	display.getCliComm().printInfo("Screen saved to " + filename);
	return filename;
}

string ScreenShotCmd::help(const vector<string>& /*tokens*/) const
{
	return
		"screenshot                   Write screenshot to file \"openmsxNNNN.png\"\n"
		"screenshot <filename>        Write screenshot to indicated file\n"
		"screenshot -prefix foo       Write screenshot to file \"fooNNNN.png\"\n"
		"screenshot -raw              320x240 raw screenshot (of MSX screen only)\n"
		"screenshot -raw -doublesize  640x480 rw screenshot (of MSX screen only)\n"
		"screenshot -with-osd         Include OSD elements in the screenshot\n";
}

void ScreenShotCmd::tabCompletion(vector<string>& tokens) const
{
	set<string> extra;
	extra.insert("-prefix");
	extra.insert("-raw");
	extra.insert("-doublesize");
	extra.insert("-with-osd");
	UserFileContext context;
	completeFileName(getCommandController(), tokens, context, extra);
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

