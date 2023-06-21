#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "IntegerSetting.hh"
#include "GlobalSettings.hh"
#include "Keys.hh"
#include "FileOperations.hh"
#include "one_of.hh"
#include "outer.hh"
#include "unreachable.hh"
#include "utf8_unchecked.hh"
#include "build-info.hh"
#include <memory>

#include <imgui_impl_sdl2.h>

namespace openmsx {

InputEventGenerator::InputEventGenerator(CommandController& commandController,
                                         EventDistributor& eventDistributor_,
                                         GlobalSettings& globalSettings_)
	: eventDistributor(eventDistributor_)
	, globalSettings(globalSettings_)
	, grabInput(
		commandController, "grabinput",
		"This setting controls if openMSX takes over mouse and keyboard input",
		false, Setting::DONT_SAVE)
	, escapeGrabCmd(commandController)
{
	setGrabInput(grabInput.getBoolean());
	eventDistributor.registerEventListener(EventType::FOCUS, *this);

#ifndef SDL_JOYSTICK_DISABLED
	SDL_JoystickEventState(SDL_ENABLE); // joysticks generate events
#endif
}

InputEventGenerator::~InputEventGenerator()
{
	eventDistributor.unregisterEventListener(EventType::FOCUS, *this);
}

void InputEventGenerator::wait()
{
	// SDL bug workaround
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_Delay(100);
	}

	if (SDL_WaitEvent(nullptr)) {
		poll();
	}
}

void InputEventGenerator::poll()
{
	// Heuristic to emulate the old SDL1 behavior:
	//
	// SDL1 had a unicode field on each KEYDOWN event. In SDL2 that
	// information is moved to the (new) SDL_TEXTINPUT events.
	//
	// Though our MSX keyboard emulation code needs to relate KEYDOWN
	// events with the associated unicode. We try to mimic this by the
	// following heuristic:
	//   When two successive events in a single batch (so, the same
	//   invocation of poll()) are KEYDOWN followed by TEXTINPUT, then copy
	//   the unicode (of the first character) of the TEXT event to the
	//   KEYDOWN event.
	// Implementing this requires a lookahead of 1 event. So the code below
	// deals with a 'current' and a 'previous' event, and keeps track of
	// whether the previous event is still pending (not yet processed).
	//
	// In a previous version we also added the constraint that these two
	// consecutive events must have the same timestamp, but that had mixed
	// results:
	// - on Linux it worked fine
	// - on Windows the timestamps sometimes did not match
	// - on Mac the timestamps mostly did not match
	// So we removed this constraint.
	//
	// We also split SDL_TEXTINPUT events into (possibly) multiple KEYDOWN
	// events because a single event type makes it easier to handle higher
	// priority listeners that can block the event for lower priority
	// listener (console > hotkey > msx).

	SDL_Event event1, event2;
	auto* prev = &event1;
	auto* curr = &event2;
	bool pending = false;

	while (SDL_PollEvent(curr)) {
		// First pass event to ImGui
		ImGui_ImplSDL2_ProcessEvent(curr);
		ImGuiIO& io = ImGui::GetIO();
		if ((io.WantCaptureMouse &&
		     curr->type == one_of(SDL_MOUSEMOTION, SDL_MOUSEWHEEL,
		                          SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP)) ||
		    (io.WantCaptureKeyboard &&
		     curr->type == one_of(SDL_KEYDOWN, SDL_KEYUP, SDL_TEXTINPUT))) {
			continue;
		}

		// Only when ImGui hasn't captured the event, pass it to openMSX
		if (pending) {
			pending = false;
			if ((prev->type == SDL_KEYDOWN) && (curr->type == SDL_TEXTINPUT)) {
				const char* utf8 = curr->text.text;
				auto unicode = utf8::unchecked::next(utf8);
				handleKeyDown(prev->key, unicode);
				if (unicode) { // possibly there are more characters
					handleText(curr->text.timestamp, utf8);
				}
				continue;
			} else {
				handle(*prev);
			}
		}
		if (curr->type == SDL_KEYDOWN) {
			pending = true;
			std::swap(curr, prev);
		} else {
			handle(*curr);
		}
	}
	if (pending) {
		handle(*prev);
	}
}

void InputEventGenerator::setNewOsdControlButtonState(unsigned newState)
{
	unsigned deltaState = osdControlButtonsState ^ newState;
	for (unsigned i = OsdControlEvent::LEFT_BUTTON;
			i <= OsdControlEvent::B_BUTTON; ++i) {
		if (deltaState & (1 << i)) {
			if (newState & (1 << i)) {
				eventDistributor.distributeEvent(OsdControlReleaseEvent(i));
			} else {
				eventDistributor.distributeEvent(OsdControlPressEvent(i));
			}
		}
	}
	osdControlButtonsState = newState;
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickAxisMotion(
	unsigned axis, int value)
{
	auto [neg_button, pos_button] = [&] {
		switch (axis) {
		case 0:
			return std::pair{1u << OsdControlEvent::LEFT_BUTTON,
			                 1u << OsdControlEvent::RIGHT_BUTTON};
		case 1:
			return std::pair{1u << OsdControlEvent::UP_BUTTON,
			                 1u << OsdControlEvent::DOWN_BUTTON};
		default:
			// Ignore all other axis (3D joysticks and flight joysticks may
			// have more than 2 axis)
			return std::pair{0u, 0u};
		}
	}();

	if (value > 0) {
		// release negative button, press positive button
		setNewOsdControlButtonState(
			(osdControlButtonsState | neg_button) & ~pos_button);
	} else if (value < 0) {
		// press negative button, release positive button
		setNewOsdControlButtonState(
			(osdControlButtonsState | pos_button) & ~neg_button);
	} else {
		// release both buttons
		setNewOsdControlButtonState(
			osdControlButtonsState | neg_button | pos_button);
	}
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickHat(int value)
{
	unsigned dir = 0;
	if (!(value & SDL_HAT_UP   )) dir |= 1 << OsdControlEvent::UP_BUTTON;
	if (!(value & SDL_HAT_DOWN )) dir |= 1 << OsdControlEvent::DOWN_BUTTON;
	if (!(value & SDL_HAT_LEFT )) dir |= 1 << OsdControlEvent::LEFT_BUTTON;
	if (!(value & SDL_HAT_RIGHT)) dir |= 1 << OsdControlEvent::RIGHT_BUTTON;
	unsigned ab = osdControlButtonsState & ((1 << OsdControlEvent::A_BUTTON) |
	                                        (1 << OsdControlEvent::B_BUTTON));
	setNewOsdControlButtonState(ab | dir);
}

void InputEventGenerator::osdControlChangeButton(bool up, unsigned changedButtonMask)
{
	auto newButtonState = up
		? osdControlButtonsState | changedButtonMask
		: osdControlButtonsState & ~changedButtonMask;
	setNewOsdControlButtonState(newButtonState);
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickButtonEvent(unsigned button, bool up)
{
	osdControlChangeButton(
		up,
		((button & 1) ? (1 << OsdControlEvent::B_BUTTON)
		              : (1 << OsdControlEvent::A_BUTTON)));
}

void InputEventGenerator::triggerOsdControlEventsFromKeyEvent(
	Keys::KeyCode keyCode, bool up, bool repeat)
{
	unsigned buttonMask = [&] {
		switch (static_cast<Keys::KeyCode>(keyCode & Keys::K_MASK)) {
		case Keys::K_LEFT:   return 1 << OsdControlEvent::LEFT_BUTTON;
		case Keys::K_RIGHT:  return 1 << OsdControlEvent::RIGHT_BUTTON;
		case Keys::K_UP:     return 1 << OsdControlEvent::UP_BUTTON;
		case Keys::K_DOWN:   return 1 << OsdControlEvent::DOWN_BUTTON;
		case Keys::K_SPACE:  return 1 << OsdControlEvent::A_BUTTON;
		case Keys::K_RETURN: return 1 << OsdControlEvent::A_BUTTON;
		case Keys::K_ESCAPE: return 1 << OsdControlEvent::B_BUTTON;
		default: return 0;
		}
	}();
	if (buttonMask) {
		if (repeat) {
			osdControlChangeButton(!up, buttonMask);
		}
		osdControlChangeButton(up, buttonMask);
	}
}

static constexpr Uint16 normalizeModifier(SDL_Keycode sym, Uint16 mod)
{
	// Apparently modifier-keys also have the corresponding
	// modifier attribute set. See here for a discussion:
	//     https://github.com/openMSX/openMSX/issues/1202
	// As a solution, on pure modifier keys, we now clear the
	// modifier attributes.
	return (sym == one_of(SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT, SDLK_LGUI,
	                      SDLK_RCTRL, SDLK_RSHIFT, SDLK_RALT, SDLK_RGUI,
	                      SDLK_MODE))
		? 0
		: mod;
}

void InputEventGenerator::handleKeyDown(const SDL_KeyboardEvent& key, uint32_t unicode)
{
	/*if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
		event = JoystickButtonDownEvent(0, 0);
		triggerOsdControlEventsFromJoystickButtonEvent(
			0, false);
		androidButtonA = true;
	} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
		event = JoystickButtonDownEvent(0, 1);
		triggerOsdControlEventsFromJoystickButtonEvent(
			1, false);
		androidButtonB = true;
	} else*/ {
		auto mod = normalizeModifier(key.keysym.sym, key.keysym.mod);
		auto [keyCode, scanCode] = Keys::getCodes(
			key.keysym.sym, mod, key.keysym.scancode, false);
		Event event = KeyDownEvent(key.timestamp, keyCode, scanCode, unicode);
		triggerOsdControlEventsFromKeyEvent(keyCode, false, key.repeat);
		eventDistributor.distributeEvent(std::move(event));
	}
}

void InputEventGenerator::handleText(uint32_t timestamp, const char* utf8)
{
	while (true) {
		auto unicode = utf8::unchecked::next(utf8);
		if (unicode == 0) return;
		eventDistributor.distributeEvent(
			KeyDownEvent(timestamp, Keys::K_NONE, unicode));
	}
}

void InputEventGenerator::handle(const SDL_Event& evt)
{
	std::optional<Event> event;
	switch (evt.type) {
	case SDL_KEYUP:
		// Virtual joystick of SDL Android port does not have joystick
		// buttons. It has however up to 6 virtual buttons that can be
		// mapped to SDL keyboard events. Two of these virtual buttons
		// will be mapped to keys SDLK_WORLD_93 and 94 and are
		// interpreted here as joystick buttons (respectively button 0
		// and 1).
		// TODO Android code should be rewritten for SDL2
		/*if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
			event = JoystickButtonUpEvent(0, 0);
			triggerOsdControlEventsFromJoystickButtonEvent(0, true);
			androidButtonA = false;
		} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
			event = JoystickButtonUpEvent(0, 1);
			triggerOsdControlEventsFromJoystickButtonEvent(1, true);
			androidButtonB = false;
		} else*/ {
			auto mod = normalizeModifier(evt.key.keysym.sym, evt.key.keysym.mod);
			auto [keyCode, scanCode] = Keys::getCodes(
				evt.key.keysym.sym, mod, evt.key.keysym.scancode, true);
			event = KeyUpEvent(evt.key.timestamp, keyCode, scanCode);
			bool repeat = false;
			triggerOsdControlEventsFromKeyEvent(keyCode, true, repeat);
		}
		break;
	case SDL_KEYDOWN:
		handleKeyDown(evt.key, 0);
		break;

	case SDL_MOUSEBUTTONUP:
		event = MouseButtonUpEvent(evt);
		break;
	case SDL_MOUSEBUTTONDOWN:
		event = MouseButtonDownEvent(evt);
		break;
	case SDL_MOUSEWHEEL:
		event = MouseWheelEvent(evt);
		break;
	case SDL_MOUSEMOTION:
		event = MouseMotionEvent(evt);
		break;

	case SDL_JOYBUTTONUP:
		event = JoystickButtonUpEvent(evt);
		triggerOsdControlEventsFromJoystickButtonEvent(
			evt.jbutton.button, true);
		break;
	case SDL_JOYBUTTONDOWN:
		event = JoystickButtonDownEvent(evt);
		triggerOsdControlEventsFromJoystickButtonEvent(
			evt.jbutton.button, false);
		break;
	case SDL_JOYAXISMOTION: {
		auto& setting = globalSettings.getJoyDeadZoneSetting(evt.jaxis.which);
		int threshold = (setting.getInt() * 32768) / 100;
		auto value = (evt.jaxis.value < -threshold) ? evt.jaxis.value
		           : (evt.jaxis.value >  threshold) ? evt.jaxis.value
		                                            : 0;
		event = JoystickAxisMotionEvent(evt);
		triggerOsdControlEventsFromJoystickAxisMotion(
			evt.jaxis.axis, value);
		break;
	}
	case SDL_JOYHATMOTION:
		event = JoystickHatEvent(evt);
		triggerOsdControlEventsFromJoystickHat(evt.jhat.value);
		break;

	case SDL_TEXTINPUT:
		handleText(evt.text.timestamp, evt.text.text);
		break;

	case SDL_WINDOWEVENT:
		if (mainWindowId && (evt.window.windowID != mainWindowId)) {
			// ignore events not for the main window
			break;
		}
		switch (evt.window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			event = FocusEvent(true);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			event = FocusEvent(false);
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			event = WindowEvent(evt);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			event = QuitEvent();
			break;
		default:
			break;
		}
		break;

	case SDL_DROPFILE:
		event = FileDropEvent(
			FileOperations::getConventionalPath(evt.drop.file));
		SDL_free(evt.drop.file);
		break;

	case SDL_QUIT:
		event = QuitEvent();
		break;

	default:
		break;
	}

#if 0
	if (event) {
		std::cerr << "SDL event converted to: " << toString(event) << '\n';
	} else {
		std::cerr << "SDL event was of unknown type, not converted to an openMSX event\n";
	}
#endif

	if (event) eventDistributor.distributeEvent(std::move(*event));
}


void InputEventGenerator::updateGrab(bool grab)
{
	escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
	setGrabInput(grab);
}

int InputEventGenerator::signalEvent(const Event& event)
{
	std::visit(overloaded{
		[&](const FocusEvent& fe) {
			switch (escapeGrabState) {
				case ESCAPE_GRAB_WAIT_CMD:
					// nothing
					break;
				case ESCAPE_GRAB_WAIT_LOST:
					if (!fe.getGain()) {
						escapeGrabState = ESCAPE_GRAB_WAIT_GAIN;
					}
					break;
				case ESCAPE_GRAB_WAIT_GAIN:
					if (fe.getGain()) {
						escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
					}
					setGrabInput(true);
					break;
				default:
					UNREACHABLE;
			}
		},
		[](const EventBase&) { UNREACHABLE; }
	}, event);
	return 0;
}

void InputEventGenerator::setGrabInput(bool grab)
{
	SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);

	// TODO is this still the correct place in SDL2
	// TODO get the SDL_window
	//SDL_Window* window = ...;
	//SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
}


// Wrap SDL joystick button functions to handle the 'fake' android joystick
// buttons. The method InputEventGenerator::handle() already takes care of fake
// events for the android joystick buttons, these two wrappers handle the direct
// joystick button state queries.
int InputEventGenerator::joystickNumButtons(SDL_Joystick* joystick)
{
	if constexpr (PLATFORM_ANDROID) {
		return 2;
	} else {
		return SDL_JoystickNumButtons(joystick);
	}
}
bool InputEventGenerator::joystickGetButton(SDL_Joystick* joystick, int button)
{
	if constexpr (PLATFORM_ANDROID) {
		switch (button) {
		case 0: return androidButtonA;
		case 1: return androidButtonB;
		default: UNREACHABLE; return false;
		}
	} else {
		return SDL_JoystickGetButton(joystick, button) != 0;
	}
}


// class EscapeGrabCmd

InputEventGenerator::EscapeGrabCmd::EscapeGrabCmd(
		CommandController& commandController_)
	: Command(commandController_, "escape_grab")
{
}

void InputEventGenerator::EscapeGrabCmd::execute(
	std::span<const TclObject> /*tokens*/, TclObject& /*result*/)
{
	auto& inputEventGenerator = OUTER(InputEventGenerator, escapeGrabCmd);
	if (inputEventGenerator.grabInput.getBoolean()) {
		inputEventGenerator.escapeGrabState =
			InputEventGenerator::ESCAPE_GRAB_WAIT_LOST;
		inputEventGenerator.setGrabInput(false);
	}
}

std::string InputEventGenerator::EscapeGrabCmd::help(
	std::span<const TclObject> /*tokens*/) const
{
	return "Temporarily release input grab.";
}

} // namespace openmsx
