#include "Display.hh"

#include "ImGuiManager.hh"
#include "Layer.hh"
#include "OutputSurface.hh"
#include "RendererFactory.hh"
#include "VideoLayer.hh"
#include "VideoSystem.hh"
#include "VideoSystemChangeListener.hh"

#include "BooleanSetting.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "EnumSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "HardwareConfig.hh"
#include "IntegerSetting.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "TclArgParser.hh"
#include "Timer.hh"
#include "Version.hh"
#include "XMLElement.hh"

#include "join.hh"
#include "narrow.hh"
#include "outer.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include "build-info.hh"

#include <algorithm>
#include <array>
#include <cassert>

namespace openmsx {

Display::Display(Reactor& reactor_)
	: RTSchedulable(reactor_.getRTScheduler())
	, screenShotCmd(reactor_.getCommandController())
	, fpsInfo(reactor_.getOpenMSXInfoCommand())
	, osdGui(reactor_.getCommandController(), *this)
	, reactor(reactor_)
	, renderSettings(reactor.getCommandController())
{
	frameDurationSum = 0;
	repeat(NUM_FRAME_DURATIONS, [&] {
		frameDurations.push_front(20);
		frameDurationSum += 20;
	});
	prevTimeStamp = Timer::getTime();

	EventDistributor& eventDistributor = reactor.getEventDistributor();
	using enum EventType;
	for (auto type : {FINISH_FRAME, SWITCH_RENDERER, MACHINE_LOADED, WINDOW}) {
		eventDistributor.registerEventListener(type, *this);
	}

	renderSettings.getRendererSetting().attach(*this);
}

Display::~Display()
{
	renderSettings.getRendererSetting().detach(*this);

	EventDistributor& eventDistributor = reactor.getEventDistributor();
	using enum EventType;
	for (auto type : {WINDOW, MACHINE_LOADED, SWITCH_RENDERER, FINISH_FRAME}) {
		eventDistributor.unregisterEventListener(type, *this);
	}

	resetVideoSystem();

	assert(listeners.empty());
}

void Display::createVideoSystem()
{
	assert(!videoSystem);
	assert(currentRenderer == RenderSettings::RendererID::UNINITIALIZED);
	assert(!switchInProgress);
	switchInProgress = true;
	doRendererSwitch(renderSettings.getRenderer());
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
	auto it = std::ranges::find_if(layers, &Layer::isActive);
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
		if ((*it)->getCoverage() == Layer::Coverage::FULL) return it;
	}
}

void Display::executeRT()
{
	repaint();
}

bool Display::signalEvent(const Event& event)
{
	std::visit(overloaded{
		[&](const FinishFrameEvent& e) {
			if (e.needRender()) {
				repaint();
				reactor.getEventDistributor().distributeEvent(FrameDrawnEvent());
			}
		},
		[&](const SwitchRendererEvent& e) {
			doRendererSwitch(e.getRenderer()); // might throw
		},
		[&](const MachineLoadedEvent& /*e*/) {
			videoSystem->updateWindowTitle();
		},
		[&](const WindowEvent& e) {
			const auto& evt = e.getSdlWindowEvent();
			if (evt.event == SDL_WINDOWEVENT_EXPOSED) {
				// Don't render too often, and certainly not when the screen
				// will anyway soon be rendered.
				repaintDelayed(100 * 1000); // 10fps
			}
			if (PLATFORM_ANDROID && e.isMainWindow() &&
			    evt.event == one_of(SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST)) {
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
				bool lost = evt.event == SDL_WINDOWEVENT_FOCUS_LOST;
				renderFrozen = lost;
			}
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);
	return false;
}

std::string Display::getWindowTitle()
{
	std::string title = Version::full();
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

gl::ivec2 Display::getWindowPosition()
{
	if (auto pos = videoSystem->getWindowPosition()) {
		storeWindowPosition(*pos);
	}
	return retrieveWindowPosition();
}

void Display::setWindowPosition(gl::ivec2 pos)
{
	storeWindowPosition(pos);
	videoSystem->setWindowPosition(pos);
}

void Display::storeWindowPosition(gl::ivec2 pos)
{
	reactor.getImGuiManager().storeWindowPosition(pos);
}

gl::ivec2 Display::retrieveWindowPosition()
{
	return reactor.getImGuiManager().retrieveWindowPosition();
}

gl::ivec2 Display::getWindowSize() const
{
	int factor = renderSettings.getScaleFactor();
	return {320 * factor, 240 * factor};
}

float Display::getFps() const
{
	return 1000000.0f * NUM_FRAME_DURATIONS / narrow_cast<float>(frameDurationSum);
}

void Display::update(const Setting& setting) noexcept
{
	assert(&setting == &renderSettings.getRendererSetting()); (void)setting;
	checkRendererSwitch();
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
	if (newRenderer != currentRenderer) {
		// don't do the actual switching in the Tcl callback
		// it seems creating and destroying Settings (= Tcl vars)
		// causes problems???
		switchInProgress = true;
		reactor.getEventDistributor().distributeEvent(SwitchRendererEvent(newRenderer));
	}
}

void Display::doRendererSwitch(RenderSettings::RendererID newRenderer)
{
	assert(switchInProgress);

	bool success = false;
	while (!success) {
		try {
			doRendererSwitch2(newRenderer);
			success = true;
		} catch (MSXException& e) {
			auto& rendererSetting = renderSettings.getRendererSetting();
			std::string errorMsg = strCat(
				"Couldn't activate renderer ",
				rendererSetting.getString(),
				": ", e.getMessage());
			// now try some things that might work against this:
			auto& scaleFactorSetting = renderSettings.getScaleFactorSetting();
			auto curVal = scaleFactorSetting.getInt();
			if (curVal == MIN_SCALE_FACTOR) {
				throw FatalError(
					e.getMessage(),
					" (and I have no other ideas to try...)"); // give up and die... :(
			}
			strAppend(errorMsg, "\nTrying to decrease scale_factor setting from ",
					curVal, " to ", curVal - 1, "...");
			scaleFactorSetting.setInt(curVal - 1);
			getCliComm().printWarning(errorMsg);
		}
	}

	switchInProgress = false;
}

void Display::doRendererSwitch2(RenderSettings::RendererID newRenderer)
{
	for (auto& l : listeners) {
		l->preVideoSystemChange();
	}

	resetVideoSystem();
	currentRenderer = newRenderer;
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
	frameDurationSum += duration - frameDurations.pop_back();
	frameDurations.push_front(duration);

	// TODO maybe revisit this later (and/or simplify other calls to repaintDelayed())
	// This ensures a minimum framerate for ImGui
	repaintDelayed(16 * 1000); // 60fps
}

void Display::repaintImpl(OutputSurface& surface)
{
	for (auto it = baseLayer(); it != end(layers); ++it) {
		if ((*it)->getCoverage() != Layer::Coverage::NONE) {
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
	auto z = layer.getZ();
	auto it = std::ranges::find_if(layers, [&](const Layer* l) { return l->getZ() > z; });
	layers.insert(it, &layer);
	layer.setDisplay(*this);
}

void Display::removeLayer(Layer& layer)
{
	layers.erase(rfind_unguarded(layers, &layer));
}

void Display::updateZ(Layer& layer)
{
	auto oldPos = rfind_unguarded(layers, &layer);
	auto z = layer.getZ();
	auto newPos = std::ranges::find_if(layers, [&](const Layer* l) { return l->getZ() >= z; });

	if (oldPos == newPos) {
		return;
	} else if (oldPos < newPos) {
		std::rotate(oldPos, oldPos + 1, newPos);
	} else {
		std::rotate(newPos, oldPos, oldPos + 1);
	}
}


// ScreenShotCmd

Display::ScreenShotCmd::ScreenShotCmd(CommandController& commandController_)
	: Command(commandController_, "openmsx::internal_screenshot")
{
}

void Display::ScreenShotCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	std::string_view prefix = "openmsx";
	bool rawShot = false;
	bool doubleSize = false;
	bool withOsd = false;
	std::string size;
	std::array info = {
		valueArg("-prefix", prefix),
		flagArg("-raw", rawShot),
		flagArg("-doublesize", doubleSize), // bwcompat, alias for -size 640
		flagArg("-with-osd", withOsd),
		valueArg("-size", size)
	};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	auto& display = OUTER(Display, screenShotCmd);
	if (doubleSize && !rawShot) {
		throw CommandException("-doublesize option can only be used in "
		                       "combination with -raw");
	}
	if (rawShot && withOsd) {
		throw CommandException("-with-osd cannot be used in "
		                       "combination with -raw");
	}
	if (!size.empty()) {
		if (!rawShot) {
			throw CommandException("-size option can only be used in "
			                       "combination with -raw");
		}
		static constexpr std::array<std::string_view, 3> validSizes = { "auto", "320", "640" };
		if (!contains(validSizes, size)) {
			throw CommandException(strCat("-size option must specify one of: ", join(validSizes, ", ")));
		}
		if (doubleSize) {
			throw CommandException("Only specify either -size or -doublesize");
		}
	}

	// backwards compatiblity
	if (doubleSize) {
		size = "640";
	}
	// default size for raw
	if (rawShot && size.empty()) {
		size = "auto";
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
	std::string filename = FileOperations::parseCommandFileArgument(
		fname, SCREENSHOT_DIR, prefix, SCREENSHOT_EXTENSION);

	if (!rawShot) {
		// take screenshot as displayed, possibly with other layers (OSD stuff, ImGUI)
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
		std::optional<unsigned> height = size == "auto" ? std::nullopt : size == "640" ? std::optional(480) : std::optional(240);
		try {
			videoLayer->takeRawScreenShot(height, filename);
		} catch (MSXException& e) {
			throw CommandException(
				"Failed to take screenshot: ", e.getMessage());
		}
	}

	result = filename;
}

std::string Display::ScreenShotCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "This is a low-level internal command, you probably want to use 'screenshot' instead.";
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
	result = display.getFps();
}

std::string Display::FpsInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx
