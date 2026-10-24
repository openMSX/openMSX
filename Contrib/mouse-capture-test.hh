// Test-only command and event observer for mouse-capture-test.py.
// Included by a generated copy of main.cc, never by the distributed emulator.
#include "Command.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "ImGuiManager.hh"
#include "InputEventGenerator.hh"
#include "JoystickDevice.hh"
#include "MSXMotherBoard.hh"
#include "PluggingController.hh"
#include "Reactor.hh"
#include "TclObject.hh"

#include <imgui.h>
#include <imgui_internal.h>
#include <SDL.h>

// Test executable links SDL statically. This optional synthetic focus path lets
// the event-routing suite run on a noninteractive Windows build desktop.
extern "C" void SDL_SetKeyboardFocus(SDL_Window* window);

namespace openmsx {

class MouseCaptureTestDriver final : public Command, private EventListener
{
public:
	explicit MouseCaptureTestDriver(Reactor& reactor_)
		: Command(reactor_.getCommandController(), "mouse_capture_test"), reactor(reactor_)
	{
		for (auto type : {EventType::MOUSE_BUTTON_DOWN, EventType::MOUSE_BUTTON_UP,
		                  EventType::MOUSE_MOTION, EventType::MOUSE_WHEEL}) {
			reactor.getEventDistributor().registerEventListener(type, *this, EventDistributor::Priority::MSX);
		}
	}
	~MouseCaptureTestDriver()
	{
		for (auto type : {EventType::MOUSE_BUTTON_DOWN, EventType::MOUSE_BUTTON_UP,
		                  EventType::MOUSE_MOTION, EventType::MOUSE_WHEEL}) {
			reactor.getEventDistributor().unregisterEventListener(type, *this);
		}
	}
	[[nodiscard]] std::string help(std::span<const TclObject>) const override
	{
		return "Test-only SDL event injection and GUI/input observations.";
	}
	void execute(std::span<const TclObject> tokens, TclObject& result) override
	{
		if (tokens.size() < 2) throw CommandException("Missing test subcommand");
		auto sub = tokens[1].getString();
		auto integer = [&](size_t index) {
			if (index >= tokens.size()) throw CommandException("Missing test argument");
			return tokens[index].getInt(getInterpreter());
		};
		auto* window = SDL_GetWindowFromID(WindowEvent::getMainWindowId());
		auto& input = reactor.getInputEventGenerator();
		if (sub == "state") {
			auto* board = reactor.getMotherBoard();
			auto* mouse = board ? dynamic_cast<JoystickDevice*>(board->getPluggingController().findPluggable("mouse")) : nullptr;
			result.addDictKeyValues(
				"capture", input.getMouseCaptureState().getValue().getString(),
				"relative", int(SDL_GetRelativeMouseMode()),
				"cursor", SDL_ShowCursor(SDL_QUERY),
				"grab", window ? int(SDL_GetWindowGrab(window)) : -1,
				"focus", window && SDL_GetKeyboardFocus() == window,
				"no_mouse", bool(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse),
				"popup", ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId),
				"buttons", mouse ? int(mouse->read(board->getCurrentTime()) & 0x30) : -1,
				"downs", downs, "ups", ups, "motions", motions, "wheels", wheels,
				"gui_focus", ImGui::GetCurrentContext()->NavWindow ? ImGui::GetCurrentContext()->NavWindow->Name : "");
		} else if (sub == "reset") {
			downs = ups = motions = wheels = 0;
		} else if (sub == "focus") {
			SDL_RaiseWindow(window);
			if (SDL_getenv("MOUSE_TEST_SYNTHETIC_FOCUS")) SDL_SetKeyboardFocus(window);
			ImGui::SetWindowFocus("MSX Display Area");
		} else if (sub == "point") {
			auto* display = ImGui::FindWindowByName("MSX Display Area");
			if (!display) throw CommandException("No MSX display window");
			auto center = display->InnerRect.GetCenter();
			auto origin = ImGui::GetMainViewport()->Pos;
			result = makeTclList(int(center.x - origin.x), int(center.y - origin.y));
		} else if (sub == "hover") {
			SDL_WarpMouseInWindow(window, integer(2), integer(3));
		} else if (sub == "layout") {
			std::string ini = "[openmsx][manager]\nmainMenuBarUndocked=" + std::to_string(integer(2)) + "\n\n";
			ImGui::LoadIniSettingsFromMemory(ini.c_str());
		} else if (sub == "statusbar") {
			reactor.getImGuiManager().statusBarVisible = integer(2) != 0;
		} else {
			SDL_Event event = {};
			event.common.timestamp = SDL_GetTicks();
			if (sub == "button") {
				event.type = integer(2) ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
				event.button.windowID = WindowEvent::getMainWindowId();
				event.button.button = uint8_t(integer(3));
				event.button.state = integer(2) ? SDL_PRESSED : SDL_RELEASED;
				event.button.x = integer(4);
				event.button.y = integer(5);
			} else if (sub == "motion") {
				event.type = SDL_MOUSEMOTION;
				event.motion.windowID = WindowEvent::getMainWindowId();
				event.motion.xrel = integer(2);
				event.motion.yrel = integer(3);
				event.motion.x = integer(4);
				event.motion.y = integer(5);
			} else if (sub == "key") {
				event.type = integer(2) ? SDL_KEYDOWN : SDL_KEYUP;
				event.key.windowID = WindowEvent::getMainWindowId();
				event.key.state = integer(2) ? SDL_PRESSED : SDL_RELEASED;
				event.key.keysym.sym = integer(3);
				event.key.keysym.scancode = SDL_GetScancodeFromKey(event.key.keysym.sym);
				event.key.keysym.mod = uint16_t(integer(4));
			} else if (sub == "lost_focus") {
				if (SDL_getenv("MOUSE_TEST_SYNTHETIC_FOCUS")) SDL_SetKeyboardFocus(nullptr);
				event.type = SDL_WINDOWEVENT;
				event.window.windowID = WindowEvent::getMainWindowId();
				event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
			} else {
				throw CommandException("Unknown test subcommand");
			}
			if (SDL_PushEvent(&event) != 1) throw CommandException(SDL_GetError());
		}
	}

private:
	bool signalEvent(const Event& event) override
	{
		switch (getType(event)) {
		case EventType::MOUSE_BUTTON_DOWN: ++downs; break;
		case EventType::MOUSE_BUTTON_UP: ++ups; break;
		case EventType::MOUSE_MOTION: ++motions; break;
		case EventType::MOUSE_WHEEL: ++wheels; break;
		default: break;
		}
		return false;
	}
	Reactor& reactor;
	unsigned downs = 0, ups = 0, motions = 0, wheels = 0;
};

} // namespace openmsx
