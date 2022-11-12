#include "Display.hh"
#include "RendererFactory.hh"
#include "Layer.hh"
#include "VideoSystem.hh"
#include "VideoLayer.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "Timer.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "EnumSetting.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "TclArgParser.hh"
#include "XMLElement.hh"
#include "VideoSystemChangeListener.hh"
#include "CommandException.hh"
#include "Version.hh"
#include "build-info.hh"
#include "narrow.hh"
#include "outer.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <array>
#include <cassert>

using std::string;

namespace openmsx {

Display::Display(Reactor& reactor_)
	: RTSchedulable(reactor_.getRTScheduler())
	, screenShotCmd(reactor_.getCommandController())
	, fpsInfo(reactor_.getOpenMSXInfoCommand())
	, osdGui(reactor_.getCommandController(), *this)
	, reactor(reactor_)
	, renderSettings(reactor.getCommandController())
	, commandConsole(reactor.getGlobalCommandController(),
	                 reactor.getEventDistributor(), *this)
{
	frameDurationSum = 0;
	repeat(NUM_FRAME_DURATIONS, [&] {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	});
	prevTimeStamp = Timer::getTime();

	EventDistributor& eventDistributor = reactor.getEventDistributor();
	eventDistributor.registerEventListener(EventType::FINISH_FRAME,
			*this);
	eventDistributor.registerEventListener(EventType::SWITCH_RENDERER,
			*this);
	eventDistributor.registerEventListener(EventType::MACHINE_LOADED,
			*this);
	eventDistributor.registerEventListener(EventType::EXPOSE,
			*this);
#if PLATFORM_ANDROID
	eventDistributor.registerEventListener(EventType::FOCUS,
			*this);
#endif
	renderSettings.getRendererSetting().attach(*this);
	renderSettings.getFullScreenSetting().attach(*this);
	renderSettings.getScaleFactorSetting().attach(*this);
	renderFrozen = false;
}

Display::~Display()
{
	renderSettings.getRendererSetting().detach(*this);
	renderSettings.getFullScreenSetting().detach(*this);
	renderSettings.getScaleFactorSetting().detach(*this);

	EventDistributor& eventDistributor = reactor.getEventDistributor();
#if PLATFORM_ANDROID
	eventDistributor.unregisterEventListener(EventType::FOCUS,
			*this);
#endif
	eventDistributor.unregisterEventListener(EventType::EXPOSE,
			*this);
	eventDistributor.unregisterEventListener(EventType::MACHINE_LOADED,
			*this);
	eventDistributor.unregisterEventListener(EventType::SWITCH_RENDERER,
			*this);
	eventDistributor.unregisterEventListener(EventType::FINISH_FRAME,
			*this);

	resetVideoSystem();

	assert(listeners.empty());
}

void Display::createVideoSystem()
{
	assert(!videoSystem);
	assert(currentRenderer == RenderSettings::UNINITIALIZED);
	assert(!switchInProgress);
	currentRenderer = renderSettings.getRenderer();
	switchInProgress = true;
	doRendererSwitch();
}

VideoSystem& Display::getVideoSystem()
{
	assert(videoSystem);
	return *videoSystem;
}

OutputSurface* Display::getOutputSurface()
{
	return videoSystem ? videoSystem->getOutputSurface() : nullptr;
}

void Display::resetVideoSystem()
{
	videoSystem.reset();
	// At this point all layers except for the Video9000 layer
	// should be gone.
	//assert(layers.empty());
}

CliComm& Display::getCliComm() const
{
	return reactor.getCliComm();
}

void Display::attach(VideoSystemChangeListener& listener)
{
	assert(!contains(listeners, &listener));
	listeners.push_back(&listener);
}

void Display::detach(VideoSystemChangeListener& listener)
{
	move_pop_back(listeners, rfind_unguarded(listeners, &listener));
}

Layer* Display::findActiveLayer() const
{
	auto it = ranges::find_if(layers, &Layer::isActive);
	return (it != layers.end()) ? *it : nullptr;
}

Display::Layers::iterator Display::baseLayer()
{
	// Note: It is possible to cache this, but since the number of layers is
	//       low at the moment, it's not really worth it.
	auto it = end(layers);
	while (true) {
		if (it == begin(layers)) {
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

void Display::executeRT()
{
	repaint();
}

int Display::signalEvent(const Event& event) noexcept
{
	visit(overloaded{
		[&](const FinishFrameEvent& e) {
			if (e.needRender()) {
				repaint();
				reactor.getEventDistributor().distributeEvent(
					Event::create<FrameDrawnEvent>());
			}
		},
		[&](const SwitchRendererEvent& /*e*/) {
			doRendererSwitch();
		},
		[&](const MachineLoadedEvent& /*e*/) {
			videoSystem->updateWindowTitle();
		},
		[&](const ExposeEvent& /*e*/) {
			// Don't render too often, and certainly not when the screen
			// will anyway soon be rendered.
			repaintDelayed(100 * 1000); // 10fps
		},
		[&](const FocusEvent& e) {
			(void)e;
			if (PLATFORM_ANDROID) {
				// On Android, the rendering must be frozen when the app is sent to
				// the background, because Android takes away all graphics resources
				// from the app. It simply destroys the entire graphics context.
				// Though, a repaint() must happen within the focus-lost event
				// so that the SDL Android port realizes that the graphics context
				// is gone and will re-build it again on the first flush to the
				// surface after the focus has been regained.

				// Perform a repaint before updating the renderFrozen flag:
				// -When loosing the focus, this repaint will flush a last
				//  time the SDL surface, making sure that the Android SDL
				//  port discovers that the graphics context is gone.
				// -When gaining the focus, this repaint does nothing as
				//  the renderFrozen flag is still false
				repaint();
				ad_printf("Setting renderFrozen to %d", !e.getGain());
				renderFrozen = !e.getGain();
			}
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);
	return 0;
}

string Display::getWindowTitle()
{
	string title = Version::full();
	if (!Version::RELEASE) {
		strAppend(title, " [", BUILD_FLAVOUR, ']');
	}
	if (MSXMotherBoard* motherboard = reactor.getMotherBoard()) {
		if (const HardwareConfig* machine = motherboard->getMachineConfig()) {
			const auto& config = machine->getConfig();
			strAppend(title, " - ",
			          config.getChild("info").getChildData("manufacturer"), ' ',
			          config.getChild("info").getChildData("code"));
		}
	}
	return title;
}

void Display::update(const Setting& setting) noexcept
{
	if (&setting == &renderSettings.getRendererSetting()) {
		checkRendererSwitch();
	} else if (&setting == &renderSettings.getFullScreenSetting()) {
		checkRendererSwitch();
	} else if (&setting == &renderSettings.getScaleFactorSetting()) {
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
	auto newRenderer = renderSettings.getRenderer();
	if ((newRenderer != currentRenderer) ||
	    !getVideoSystem().checkSettings()) {
		currentRenderer = newRenderer;
		// don't do the actual switching in the Tcl callback
		// it seems creating and destroying Settings (= Tcl vars)
		// causes problems???
		switchInProgress = true;
		reactor.getEventDistributor().distributeEvent(
			Event::create<SwitchRendererEvent>());
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
			auto& rendererSetting = renderSettings.getRendererSetting();
			string errorMsg = strCat(
				"Couldn't activate renderer ",
				rendererSetting.getString(),
				": ", e.getMessage());
			// now try some things that might work against this:
			if (rendererSetting.getEnum() != RenderSettings::SDL) {
				errorMsg += "\nTrying to switch to SDL renderer instead...";
				rendererSetting.setEnum(RenderSettings::SDL);
				currentRenderer = RenderSettings::SDL;
			} else {
				auto& scaleFactorSetting = renderSettings.getScaleFactorSetting();
				auto curVal = scaleFactorSetting.getInt();
				if (curVal == 1) {
					throw MSXException(
						e.getMessage(),
						" (and I have no other ideas to try...)"); // give up and die... :(
				}
				strAppend(errorMsg, "\nTrying to decrease scale_factor setting from ",
				          curVal, " to ", curVal - 1, "...");
				scaleFactorSetting.setInt(curVal - 1);
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

	for (auto& l : listeners) {
		l->postVideoSystemChange();
	}
}

void Display::repaintImpl()
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

	cancelRT(); // cancel delayed repaint

	if (!renderFrozen) {
		assert(videoSystem);
		if (OutputSurface* surface = videoSystem->getOutputSurface()) {
			repaintImpl(*surface);
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

void Display::repaintImpl(OutputSurface& surface)
{
	for (auto it = baseLayer(); it != end(layers); ++it) {
		if ((*it)->getCoverage() != Layer::COVER_NONE) {
			(*it)->paint(surface);
		}
	}
}

void Display::repaint()
{
	// Request a repaint from the VideoSystem. This may call repaintImpl()
	// directly or for example defer to a signal callback on VisibleSurface.
	videoSystem->repaint();
}

void Display::repaintDelayed(uint64_t delta)
{
	if (isPendingRT()) {
		// already a pending repaint
		return;
	}
	scheduleRT(unsigned(delta));
}

void Display::addLayer(Layer& layer)
{
	int z = layer.getZ();
	auto it = ranges::find_if(layers, [&](Layer* l) { return l->getZ() > z; });
	layers.insert(it, &layer);
	layer.setDisplay(*this);
}

void Display::removeLayer(Layer& layer)
{
	layers.erase(rfind_unguarded(layers, &layer));
}

void Display::updateZ(Layer& layer) noexcept
{
	// Remove at old Z-index...
	removeLayer(layer);
	// ...and re-insert at new Z-index.
	addLayer(layer);
}


// ScreenShotCmd

Display::ScreenShotCmd::ScreenShotCmd(CommandController& commandController_)
	: Command(commandController_, "screenshot")
{
}

void Display::ScreenShotCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	std::string_view prefix = "openmsx";
	bool rawShot = false;
	bool msxOnly = false;
	bool doubleSize = false;
	bool withOsd = false;
	std::array info = {
		valueArg("-prefix", prefix),
		flagArg("-raw", rawShot),
		flagArg("-msxonly", msxOnly),
		flagArg("-doublesize", doubleSize),
		flagArg("-with-osd", withOsd)
	};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	auto& display = OUTER(Display, screenShotCmd);
	if (msxOnly) {
		display.getCliComm().printWarning(
			"The -msxonly option has been deprecated and will "
			"be removed in a future release. Instead, use the "
			"-raw option for the same effect.");
		rawShot = true;
	}
	if (doubleSize && !rawShot) {
		throw CommandException("-doublesize option can only be used in "
		                       "combination with -raw");
	}
	if (rawShot && withOsd) {
		throw CommandException("-with-osd cannot be used in "
		                       "combination with -raw");
	}

	std::string_view fname;
	switch (arguments.size()) {
	case 0:
		// nothing
		break;
	case 1:
		fname = arguments[0].getString();
		break;
	default:
		throw SyntaxError();
	}
	string filename = FileOperations::parseCommandFileArgument(
		fname, "screenshots", prefix, ".png");

	if (!rawShot) {
		// include all layers (OSD stuff, console)
		try {
			display.getVideoSystem().takeScreenShot(filename, withOsd);
		} catch (MSXException& e) {
			throw CommandException(
				"Failed to take screenshot: ", e.getMessage());
		}
	} else {
		auto* videoLayer = dynamic_cast<VideoLayer*>(
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
				"Failed to take screenshot: ", e.getMessage());
		}
	}

	display.getCliComm().printInfo("Screen saved to ", filename);
	result = filename;
}

string Display::ScreenShotCmd::help(std::span<const TclObject> /*tokens*/) const
{
	// Note: -no-sprites option is implemented in Tcl
	return "screenshot                   Write screenshot to file \"openmsxNNNN.png\"\n"
	       "screenshot <filename>        Write screenshot to indicated file\n"
	       "screenshot -prefix foo       Write screenshot to file \"fooNNNN.png\"\n"
	       "screenshot -raw              320x240 raw screenshot (of MSX screen only)\n"
	       "screenshot -raw -doublesize  640x480 raw screenshot (of MSX screen only)\n"
	       "screenshot -with-osd         Include OSD elements in the screenshot\n"
	       "screenshot -no-sprites       Don't include sprites in the screenshot\n";
}

void Display::ScreenShotCmd::tabCompletion(std::vector<string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array extra = {
		"-prefix"sv, "-raw"sv, "-doublesize"sv, "-with-osd"sv, "-no-sprites"sv,
	};
	completeFileName(tokens, userFileContext(), extra);
}


// FpsInfoTopic

Display::FpsInfoTopic::FpsInfoTopic(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "fps")
{
}

void Display::FpsInfoTopic::execute(std::span<const TclObject> /*tokens*/,
                                    TclObject& result) const
{
	auto& display = OUTER(Display, fpsInfo);
	result = 1000000.0f * Display::NUM_FRAME_DURATIONS / narrow_cast<float>(display.frameDurationSum);
}

string Display::FpsInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx
