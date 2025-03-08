#include "Event.hh"

#include "stl.hh"
#include "strCat.hh"

#include <SDL3/SDL.h>

#include <tuple>

using namespace std::literals;

namespace openmsx {

[[nodiscard]] static constexpr uint16_t normalizeKeyMod(uint16_t m)
{
	// when either left or right modifier is pressed, add the other one as well
	if (m & SDL_KMOD_SHIFT) m |= SDL_KMOD_SHIFT;
	if (m & SDL_KMOD_CTRL)  m |= SDL_KMOD_CTRL;
	if (m & SDL_KMOD_ALT)   m |= SDL_KMOD_ALT;
	if (m & SDL_KMOD_GUI)   m |= SDL_KMOD_GUI;
	// ignore stuff like: SDL_KMOD_NUM, SDL_KMOD_CAPS, SDL_KMOD_SCROLL
	m &= (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI | SDL_KMOD_MODE);
	return m;
}

bool operator==(const Event& x, const Event& y)
{
	return std::visit(overloaded{
		[](const KeyUpEvent& a, const KeyUpEvent& b) {
			// note: don't compare scancode, unicode
			return std::tuple(a.getKeyCode(), normalizeKeyMod(a.getModifiers())) ==
			       std::tuple(b.getKeyCode(), normalizeKeyMod(b.getModifiers()));
		},
		[](const KeyDownEvent& a, const KeyDownEvent& b) {
			// note: don't compare scancode, unicode
			return std::tuple(a.getKeyCode(), normalizeKeyMod(a.getModifiers())) ==
			       std::tuple(b.getKeyCode(), normalizeKeyMod(b.getModifiers()));
		},
		[](const MouseMotionEvent& a, const MouseMotionEvent& b) {
			return std::tuple(a.getX(), a.getY(), a.getAbsX(), a.getAbsY()) ==
			       std::tuple(b.getX(), b.getY(), b.getAbsX(), b.getAbsY());
		},
		[](const MouseButtonUpEvent& a, const MouseButtonUpEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const MouseButtonDownEvent& a, const MouseButtonDownEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const MouseWheelEvent& a, const MouseWheelEvent& b) {
			return std::tuple(a.getX(), a.getY()) ==
			       std::tuple(b.getX(), b.getY());
		},
		[](const JoystickAxisMotionEvent& a, const JoystickAxisMotionEvent& b) {
			return std::tuple(a.getJoystick(), a.getAxis(), a.getValue()) ==
			       std::tuple(b.getJoystick(), b.getAxis(), b.getValue());
		},
		[](const JoystickHatEvent& a, const JoystickHatEvent& b) {
			return std::tuple(a.getJoystick(), a.getHat(), a.getValue()) ==
			       std::tuple(b.getJoystick(), b.getHat(), b.getValue());
		},
		[](const JoystickButtonUpEvent& a, const JoystickButtonUpEvent& b) {
			return std::tuple(a.getJoystick(), a.getButton()) ==
			       std::tuple(b.getJoystick(), b.getButton());
		},
		[](const JoystickButtonDownEvent& a, const JoystickButtonDownEvent& b) {
			return std::tuple(a.getJoystick(), a.getButton()) ==
			       std::tuple(b.getJoystick(), b.getButton());
		},
		[](const OsdControlReleaseEvent& a, const OsdControlReleaseEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const OsdControlPressEvent& a, const OsdControlPressEvent& b) {
			return a.getButton() == b.getButton();
		},
		[](const WindowEvent& a_, const WindowEvent& b_) {
			const auto& a = a_.getSdlWindowEvent();
			const auto& b = b_.getSdlWindowEvent();
			// don't compare timestamp
			if (a.event != b.event) return false;
			auto getWindowId = [](const SDL_WindowEvent& e) {
				return e.windowID == Uint32(-1) ? WindowEvent::getMainWindowId() : e.windowID;
			};
			if (getWindowId(a) != getWindowId(b)) return false;
			// TODO for specific events, compare data1 and data2
			return true;
		},
		[](const FileDropEvent& a, const FileDropEvent& b) {
			return a.getFileName() == b.getFileName();
		},
		[](const FinishFrameEvent& a, const FinishFrameEvent& b) {
			return std::tuple(a.getSource(), a.getSelectedSource(), a.isSkipped()) ==
			       std::tuple(b.getSource(), b.getSelectedSource(), b.isSkipped());
		},
		[](const CliCommandEvent& a, const CliCommandEvent& b) {
			return a.getCommand() == b.getCommand();
		},
		[](const GroupEvent& a, const GroupEvent& b) {
			return a.getTclListComponents() ==
			       b.getTclListComponents();
		},
		[&](const EventBase& /*a*/, const EventBase& /*b*/) {
			return getType(x) == getType(y);
		}
	}, x, y);
}

TclObject toTclList(const Event& event)
{
	static constexpr array_with_enum_index<OsdControlEvent::Button, std::string_view> osdControlNames = {
		"LEFT"sv, "RIGHT"sv, "UP"sv, "DOWN"sv, "A"sv, "B"sv
	};

	return std::visit(overloaded{
		[](const KeyEvent& e) {
			// Note: 'scanCode' is not included (also not in operator==()).
			//
			// At the moment 'scanCode' is only used in the MSX Keyboard code when
			// the POSITIONAL mapping mode is active. It is not used for key
			// bindings (the 'bind' or the 'after' commands) or for the openMSX
			// console. KeyEvents also don't end up in 'reverse replay' files
			// (instead those files contain more low level MSX keyboard matrix
			// changes).
			//
			// Within these constraints it's fine to ignore 'scanCode' in this
			// method.
			auto result = makeTclList("keyb", e.getKey().toString());
			if (e.getUnicode() != 0) {
				result.addListElement(tmpStrCat("unicode", e.getUnicode()));
			}
			return result;
		},
		[](const MouseMotionEvent& e) {
			return makeTclList("mouse", "motion", e.getX(), e.getY(), e.getAbsX(), e.getAbsY());
		},
		[](const MouseButtonUpEvent& e) {
			return makeTclList("mouse", tmpStrCat("button", e.getButton()), "up");
		},
		[](const MouseButtonDownEvent& e) {
			return makeTclList("mouse", tmpStrCat("button", e.getButton()), "down");
		},
		[](const MouseWheelEvent& e) {
			return makeTclList("mouse", "wheel", e.getX(), e.getY());
		},
		[](const JoystickAxisMotionEvent& e) {
			return makeTclList(e.getJoystick().str(), tmpStrCat("axis", e.getAxis()), e.getValue());
		},
		[](const JoystickHatEvent& e) {
			auto str = [&] {
				switch (e.getValue()) {
					case SDL_HAT_UP:        return "up"sv;
					case SDL_HAT_RIGHT:     return "right"sv;
					case SDL_HAT_DOWN:      return "down"sv;
					case SDL_HAT_LEFT:      return "left"sv;
					case SDL_HAT_RIGHTUP:   return "rightup"sv;
					case SDL_HAT_RIGHTDOWN: return "rightdown"sv;
					case SDL_HAT_LEFTUP:    return "leftup"sv;
					case SDL_HAT_LEFTDOWN:  return "leftdown"sv;
					default:                return "center"sv;
				}
			}();
			return makeTclList(e.getJoystick().str(), tmpStrCat("hat", e.getHat()), str);
		},
		[](const JoystickButtonUpEvent& e) {
			return makeTclList(e.getJoystick().str(), tmpStrCat("button", e.getButton()), "up");
		},
		[](const JoystickButtonDownEvent& e) {
			return makeTclList(e.getJoystick().str(), tmpStrCat("button", e.getButton()), "down");
		},
		[](const OsdControlReleaseEvent& e) {
			return makeTclList("OSDcontrol", osdControlNames[e.getButton()], "RELEASE");
		},
		[](const OsdControlPressEvent& e) {
			return makeTclList("OSDcontrol", osdControlNames[e.getButton()], "PRESS");
		},
		[](const WindowEvent& e_) {
			const auto& e = e_.getSdlWindowEvent();
			if (e.event == SDL_EVENT_WINDOW_FOCUS_GAINED) {
				return makeTclList("focus", true);
			} else if (e.event == SDL_EVENT_WINDOW_FOCUS_LOST) {
				return makeTclList("focus", false);
			}
			return makeTclList(); // other events don't need a textual representation (yet)
		},
		[](const TextEvent&) {
			return makeTclList(); // doesn't need a textual representation (yet)
		},
		[](const FileDropEvent& e) {
			return makeTclList("filedrop", e.getFileName());
		},
		[](const QuitEvent& /*e*/) {
			return makeTclList("quit");
		},
		[](const FinishFrameEvent& e) {
			return makeTclList("finishframe", e.getSource(), e.getSelectedSource(), e.isSkipped());
		},
		[](const CliCommandEvent& e) {
			return makeTclList("CliCmd", e.getCommand());
		},
		[](const GroupEvent& e) {
			return e.getTclListComponents();
		},
		[&](const SimpleEvent& /*e*/) {
			return makeTclList("simple", int(getType(event)));
		},
		[](const ImGuiActiveEvent& e) {
			return makeTclList("imgui", e.getActive());
		},
		[](const SwitchRendererEvent& e) {
			return makeTclList("renderer", std::to_underlying(e.getRenderer()));
		}
	}, event);
}

std::string toString(const Event& event)
{
	return std::string(toTclList(event).getString());
}

bool matches(const Event& self, const Event& other)
{
	return std::visit(overloaded{
		[&](const GroupEvent& e) {
			return contains(e.getTypesToMatch(), getType(other));
		},
		[&](const EventBase&) {
			return self == other;
		}
	}, self);
}

} // namespace openmsx
