// $Id$

#include "Display.hh"
#include "Layer.hh"
#include "VideoSystem.hh"
#include "PostProcessor.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "InputEvents.hh"
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
#include "EnumSetting.hh"
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
#include "memory.hh"
#include "openmsx.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class ScreenShotCmd : public Command
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
	virtual void execute(const vector<TclObject>& tokens,
			     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	Display& display;
};


Display::Display(Reactor& reactor_)
	: alarm(make_unique<AlarmEvent>(
		reactor_.getEventDistributor(), *this,
		OPENMSX_DELAYED_REPAINT_EVENT))
	, screenShotCmd(make_unique<ScreenShotCmd>(
		reactor_.getCommandController(), *this))
	, fpsInfo(make_unique<FpsInfoTopic>(
		reactor_.getOpenMSXInfoCommand(), *this))
	, osdGui(make_unique<OSDGUI>(
		reactor_.getCommandController(), *this))
	, reactor(reactor_)
	, renderSettings(make_unique<RenderSettings>(
		reactor.getCommandController()))
	, commandConsole(make_unique<CommandConsole>(
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
#if PLATFORM_ANDROID
	eventDistributor.registerEventListener(OPENMSX_FOCUS_EVENT,
			*this);
#endif
	renderSettings->getRenderer().attach(*this);
	renderSettings->getFullScreen().attach(*this);
	renderSettings->getScaleFactor().attach(*this);
	renderFrozen = false;
}

Display::~Display()
{
	renderSettings->getRenderer().detach(*this);
	renderSettings->getFullScreen().detach(*this);
	renderSettings->getScaleFactor().detach(*this);

	EventDistributor& eventDistributor = reactor.getEventDistributor();
#if PLATFORM_ANDROID
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT,
			*this);
#endif
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
	// At this point all layers expect for the Video9000 layer
	// should be gone.
	//assert(layers.empty());
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
	auto it = find(listeners.begin(), listeners.end(), &listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

Layer* Display::findActiveLayer() const
{
	for (auto& l : layers) {
		if (l->getZ() == Layer::Z_MSX_ACTIVE) {
			return l;
		}
	}
	return nullptr;
}

Display::Layers::iterator Display::baseLayer()
{
	// Note: It is possible to cache this, but since the number of layers is
	//       low at the moment, it's not really worth it.
	auto it = layers.end();
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

int Display::signalEvent(const std::shared_ptr<const Event>& event)
{
	if (event->getType() == OPENMSX_FINISH_FRAME_EVENT) {
		auto& ffe = checked_cast<const FinishFrameEvent&>(*event);
		if (ffe.needRender()) {
			repaint();
			reactor.getEventDistributor().distributeEvent(
				std::make_shared<SimpleEvent>(
					OPENMSX_FRAME_DRAWN_EVENT));
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
	} else if (PLATFORM_ANDROID && event->getType() == OPENMSX_FOCUS_EVENT) {
		// On Android, the rendering must be frozen when the app is sent to
		// the background, because Android takes away all graphics resources
		// from the app. It simply destroys the entire graphics context.
		// Though, a repaint() must happen within the focus-lost event
		// so that the SDL Android port realises that the graphix context
		// is gone and will re-build it again on the first flush to the
		// surface after the focus has been regained.

		// Perform a repaint before updating the renderFrozen flag:
		// -When loosing the focus, this repaint will flush a last
		//  time the SDL surface, making sure that the Android SDL
		//  port discovers that the graphics context is gone.
		// -When gaining the focus, this repaint does nothing as
		//  the renderFrozen flag is still false
		repaint();
		auto& focusEvent = checked_cast<const FocusEvent&>(*event);
		ad_printf("Setting renderFrozen to %d", !focusEvent.getGain());
		renderFrozen = !focusEvent.getGain();
	}
	return 0;
}

void Display::setWindowTitle()
{
	string title = Version::full();
	if (!Version::RELEASE) {
		title += " [";
		title += BUILD_FLAVOUR;
		title += ']';
	}
	if (MSXMotherBoard* motherboard = reactor.getMotherBoard()) {
		if (const HardwareConfig* machine = motherboard->getMachineConfig()) {
			const XMLElement& config = machine->getConfig();
			title += " - " +
			    config.getChild("info").getChildData("manufacturer") + ' ' +
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
	} else {
		UNREACHABLE;
	}
}

void Display::checkRendererSwitch()
{
	if (switchInProgress) {
		// This method only queues a request to switch renderer (see
		// comments below why). If there already is such a request
		// queued we don't need to do it again.
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
			std::make_shared<SimpleEvent>(
				OPENMSX_SWITCH_RENDERER_EVENT));
	}
}

void Display::doRendererSwitch()
{
	assert(switchInProgress);

	bool success = false;
	while (!success) {
		try {
			doRendererSwitch2();
			success = true;
		} catch (MSXException& e) {
			string errorMsg = "Couldn't activate renderer " +
				renderSettings->getRenderer().getValueString() +
				": " + e.getMessage();
			// now try some things that might work against this:
			if (renderSettings->getRenderer().getValue() != RendererFactory::SDL) {
				errorMsg += "\nTrying to switch to SDL renderer instead...";
				renderSettings->getRenderer().changeValue(RendererFactory::SDL);
				currentRenderer = RendererFactory::SDL;
			} else {
				unsigned curval = renderSettings->getScaleFactor().getValue();
				if (curval == 1) throw MSXException(e.getMessage() + " (and I have no other ideas to try...)"); // give up and die... :(
				errorMsg += "\nTrying to decrease scale_factor setting from " + StringOp::toString(curval) + " to " + StringOp::toString(curval - 1) + "...";
				renderSettings->getScaleFactor().changeValue(curval - 1);
			}
			getCliComm().printWarning(errorMsg);
		}
	}

	switchInProgress = false;
}

void Display::doRendererSwitch2()
{
	for (auto& l : listeners) {
		l->preVideoSystemChange();
	}

	resetVideoSystem();
	videoSystem = RendererFactory::createVideoSystem(reactor);
	setWindowTitle();

	for (auto& l : listeners) {
		l->postVideoSystemChange();
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

	if (!renderFrozen) {
		assert(videoSystem.get());
		if (OutputSurface* surface = videoSystem->getOutputSurface()) {
			repaint(*surface);
			videoSystem->flush();
		}
	}

	// update fps statistics
	auto now = Timer::getTime();
	auto duration = now - prevTimeStamp;
	prevTimeStamp = now;
	frameDurationSum += duration - frameDurations.removeBack();
	frameDurations.addFront(duration);
}

void Display::repaint(OutputSurface& surface)
{
	for (auto it = baseLayer(); it != layers.end(); ++it) {
		if ((*it)->getCoverage() != Layer::COVER_NONE) {
			(*it)->paint(surface);
		}
	}
}

void Display::repaintDelayed(uint64_t delta)
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
	auto it = layers.begin();
	while (it != layers.end() && (*it)->getZ() < z) {
		++it;
	}

	layers.insert(it, &layer);
	layer.setDisplay(*this);
}

void Display::removeLayer(Layer& layer)
{
	auto it = std::find(layers.begin(), layers.end(), &layer);
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
	: Command(commandController, "screenshot")
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
		if (StringOp::startsWith(tokens[i], '-')) {
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
		// nothing
		break;
	case 1:
		filename = arguments[0];
		break;
	default:
		throw SyntaxError();
	}
	filename = FileOperations::parseCommandFileArgument(
		filename, "screenshots", prefix, ".png");

	if (!rawShot) {
		// include all layers (OSD stuff, console)
		try {
			display.getVideoSystem().takeScreenShot(filename, withOsd);
		} catch (MSXException& e) {
			throw CommandException(
				"Failed to take screenshot: " + e.getMessage());
		}
	} else {
		auto videoLayer = dynamic_cast<VideoLayer*>(
			display.findActiveLayer());
		if (!videoLayer) {
			throw CommandException(
				"Current renderer doesn't support taking screenshots.");
		}
		unsigned height = doubleSize ? 480 : 240;
		try {
			videoLayer->takeRawScreenShot(height, filename);
		} catch (MSXException& e) {
			throw CommandException(
				"Failed to take screenshot: " + e.getMessage());
		}
	}

	display.getCliComm().printInfo("Screen saved to " + filename);
	return filename;
}

string ScreenShotCmd::help(const vector<string>& /*tokens*/) const
{
	// Note: -no-sprites option is implemented in Tcl
	return
		"screenshot                   Write screenshot to file \"openmsxNNNN.png\"\n"
		"screenshot <filename>        Write screenshot to indicated file\n"
		"screenshot -prefix foo       Write screenshot to file \"fooNNNN.png\"\n"
		"screenshot -raw              320x240 raw screenshot (of MSX screen only)\n"
		"screenshot -raw -doublesize  640x480 raw screenshot (of MSX screen only)\n"
		"screenshot -with-osd         Include OSD elements in the screenshot\n"
		"screenshot -no-sprites       Don't include sprites in the screenshot\n";
}

void ScreenShotCmd::tabCompletion(vector<string>& tokens) const
{
	static const char* const extra[] = {
		"-prefix", "-raw", "-doublesize", "-with-osd", "-no-sprites",
	};
	completeFileName(tokens, UserFileContext(), extra);
}


// FpsInfoTopic

FpsInfoTopic::FpsInfoTopic(InfoCommand& openMSXInfoCommand,
                           Display& display_)
	: InfoTopic(openMSXInfoCommand, "fps")
	, display(display_)
{
}

void FpsInfoTopic::execute(const vector<TclObject>& /*tokens*/,
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

