#include "InputEventGenerator.hh"

#include "Event.hh"
#include "EventDistributor.hh"
#include "FileOperations.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "SDLKey.hh"

#include "one_of.hh"
#include "outer.hh"
#include "unreachable.hh"
#include "utf8_unchecked.hh"

#include "build-info.hh"

#include <memory>

namespace openmsx {

InputEventGenerator::InputEventGenerator(CommandController& commandController,
                                         EventDistributor& eventDistributor_,
                                         GlobalSettings& globalSettings_)
	: eventDistributor(eventDistributor_)
	, globalSettings(globalSettings_)
	, joystickManager(commandController)
	, grabInput(
		commandController, "grabinput",
		"This setting controls if openMSX takes over mouse and keyboard input",
		false, Setting::Save::NO)
	, escapeGrabCmd(commandController)
{
	setGrabInput(grabInput.getBoolean());
	eventDistributor.registerEventListener(EventType::WINDOW, *this);
}

InputEventGenerator::~InputEventGenerator()
{
	eventDistributor.unregisterEventListener(EventType::WINDOW, *this);
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
		if (pending) {
			pending = false;
			if ((prev->type == SDL_KEYDOWN) && (curr->type == SDL_TEXTINPUT)) {
				const char* utf8 = curr->text.text;
				auto unicode = utf8::unchecked::next(utf8);
				handleKeyDown(prev->key, unicode);
				if (unicode) { // possibly there are more characters
					splitText(curr->text.timestamp, utf8);
				}
				eventDistributor.distributeEvent(TextEvent(*curr));
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
	using enum OsdControlEvent::Button;
	for (auto b : {LEFT, RIGHT, UP, DOWN, A, B}) {
		if (deltaState & (1 << to_underlying(b))) {
			if (newState & (1 << to_underlying(b))) {
				eventDistributor.distributeEvent(OsdControlReleaseEvent(b));
			} else {
				eventDistributor.distributeEvent(OsdControlPressEvent(b));
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
		using enum OsdControlEvent::Button;
		case 0:
			return std::pair{1u << to_underlying(LEFT),
			                 1u << to_underlying(RIGHT)};
		case 1:
			return std::pair{1u << to_underlying(UP),
			                 1u << to_underlying(DOWN)};
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
	using enum OsdControlEvent::Button;
	unsigned dir = 0;
	if (!(value & SDL_HAT_UP   )) dir |= 1 << to_underlying(UP);
	if (!(value & SDL_HAT_DOWN )) dir |= 1 << to_underlying(DOWN);
	if (!(value & SDL_HAT_LEFT )) dir |= 1 << to_underlying(LEFT);
	if (!(value & SDL_HAT_RIGHT)) dir |= 1 << to_underlying(RIGHT);
	unsigned ab = osdControlButtonsState & ((1 << to_underlying(A)) |
	                                        (1 << to_underlying(B)));
	setNewOsdControlButtonState(ab | dir);
}

void InputEventGenerator::osdControlChangeButton(bool down, unsigned changedButtonMask)
{
	auto newButtonState = down
		? osdControlButtonsState & ~changedButtonMask
		: osdControlButtonsState | changedButtonMask;
	setNewOsdControlButtonState(newButtonState);
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickButtonEvent(unsigned button, bool down)
{
	using enum OsdControlEvent::Button;
	osdControlChangeButton(
		down,
		((button & 1) ? (1 << to_underlying(B))
		              : (1 << to_underlying(A))));
}

void InputEventGenerator::triggerOsdControlEventsFromKeyEvent(SDLKey key, bool repeat)
{
	unsigned buttonMask = [&] {
		switch (key.sym.sym) {
		using enum OsdControlEvent::Button;
		case SDLK_LEFT:   return 1 << to_underlying(LEFT);
		case SDLK_RIGHT:  return 1 << to_underlying(RIGHT);
		case SDLK_UP:     return 1 << to_underlying(UP);
		case SDLK_DOWN:   return 1 << to_underlying(DOWN);
		case SDLK_SPACE:  return 1 << to_underlying(A);
		case SDLK_RETURN: return 1 << to_underlying(A);
		case SDLK_ESCAPE: return 1 << to_underlying(B);
		default: return 0;
		}
	}();
	if (buttonMask) {
		if (repeat) {
			osdControlChangeButton(!key.down, buttonMask);
		}
		osdControlChangeButton(key.down, buttonMask);
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
		SDL_Event evt;
		evt.key = SDL_KeyboardEvent{};
		memcpy(&evt.key, &key, sizeof(key));
		evt.key.keysym.mod = normalizeModifier(key.keysym.sym, key.keysym.mod);
		evt.key.keysym.unused = unicode;
		Event event = KeyDownEvent(evt);
		triggerOsdControlEventsFromKeyEvent(SDLKey{key.keysym, true}, key.repeat);
		eventDistributor.distributeEvent(std::move(event));
	}
}

void InputEventGenerator::splitText(uint32_t timestamp, const char* utf8)
{
	while (true) {
		auto unicode = utf8::unchecked::next(utf8);
		if (unicode == 0) return;
		eventDistributor.distributeEvent(
			KeyDownEvent::create(timestamp, unicode));
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
			SDL_Event e;
			e.key = SDL_KeyboardEvent{};
			memcpy(&e.key, &evt.key, sizeof(evt.key));
			e.key.keysym.mod = normalizeModifier(evt.key.keysym.sym, evt.key.keysym.mod);
			event = KeyUpEvent(e);
			bool down = false;
			bool repeat = false;
			triggerOsdControlEventsFromKeyEvent(SDLKey{e.key.keysym, down}, repeat);
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
		if (auto* window = SDL_GL_GetCurrentWindow(); SDL_GetWindowGrab(window)) {
			int w, h;
			SDL_GetWindowSize(window, &w, &h);
			int x, y;
			SDL_GetMouseState(&x, &y);
			// There seems to be a bug in Windows in which the mouse can be locked on the right edge
			// only when moving fast to the right (not so fast actually) when grabbing is enabled
			// which stops generating mouse events
			// When moving to the left, events resume, and then moving even slower to the right fixes it
			// This only occurs when grabbing is explicitly enabled in windowed mode,
			// not in fullscreen mode (though not sure what happens with multiple monitors)
			// To reduce the impact of this bug, long range warping (e.g. to the middle of the window)
			// was attempted but that caused race conditions with fading in of gui elements
			// So, in the end it was decided that to go for the least kind of trouble
			// The value of 10 below is a heuristic value which seems to balance all factors
			// such as font size and the overall size of gui elements
			// and the speed of crossing virtual barriers
			static constexpr int MARGIN = 10;
			int xn = std::clamp(x, MARGIN, w - 1 - MARGIN);
			int yn = std::clamp(y, MARGIN, h - 1 - MARGIN);
			if (xn != x || yn != y) SDL_WarpMouseInWindow(window, xn, yn);
		}
		break;
	case SDL_JOYBUTTONUP:
		if (joystickManager.translateSdlInstanceId(const_cast<SDL_Event&>(evt))) {
			event = JoystickButtonUpEvent(evt);
			triggerOsdControlEventsFromJoystickButtonEvent(
				evt.jbutton.button, false);
		}
		break;
	case SDL_JOYBUTTONDOWN:
		if (joystickManager.translateSdlInstanceId(const_cast<SDL_Event&>(evt))) {
			event = JoystickButtonDownEvent(evt);
			triggerOsdControlEventsFromJoystickButtonEvent(
				evt.jbutton.button, true);
		}
		break;
	case SDL_JOYAXISMOTION: {
		if (auto joyId = joystickManager.translateSdlInstanceId(const_cast<SDL_Event&>(evt))) {
			const auto* setting = joystickManager.getJoyDeadZoneSetting(*joyId);
			assert(setting);
			int deadZone = setting->getInt();
			int threshold = (deadZone * 32768) / 100;
			auto value = (evt.jaxis.value < -threshold) ? evt.jaxis.value
				: (evt.jaxis.value >  threshold) ? evt.jaxis.value
								: 0;
			event = JoystickAxisMotionEvent(evt);
			triggerOsdControlEventsFromJoystickAxisMotion(
				evt.jaxis.axis, value);
		}
		break;
	}
	case SDL_JOYHATMOTION:
		if (auto joyId = joystickManager.translateSdlInstanceId(const_cast<SDL_Event&>(evt))) {
			event = JoystickHatEvent(evt);
			triggerOsdControlEventsFromJoystickHat(evt.jhat.value);
		}
		break;

	case SDL_JOYDEVICEADDED:
		joystickManager.add(evt.jdevice.which);
		break;

	case SDL_JOYDEVICEREMOVED:
		joystickManager.remove(evt.jdevice.which);
		break;

	case SDL_TEXTINPUT:
		splitText(evt.text.timestamp, evt.text.text);
		event = TextEvent(evt);
		break;

	case SDL_WINDOWEVENT:
		switch (evt.window.event) {
		case SDL_WINDOWEVENT_CLOSE:
			if (WindowEvent::isMainWindow(evt.window.windowID)) {
				event = QuitEvent();
				break;
			}
			[[fallthrough]];
		default:
			event = WindowEvent(evt);
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

bool InputEventGenerator::signalEvent(const Event& event)
{
	std::visit(overloaded{
		[&](const WindowEvent& e) {
			const auto& evt = e.getSdlWindowEvent();
			if (e.isMainWindow() &&
			    evt.event == one_of(SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST)) {
				switch (escapeGrabState) {
					case ESCAPE_GRAB_WAIT_CMD:
						// nothing
						break;
					case ESCAPE_GRAB_WAIT_LOST:
						if (evt.event == SDL_WINDOWEVENT_FOCUS_LOST) {
							escapeGrabState = ESCAPE_GRAB_WAIT_GAIN;
						}
						break;
					case ESCAPE_GRAB_WAIT_GAIN:
						if (evt.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
							escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
						}
						setGrabInput(true);
						break;
					default:
						UNREACHABLE;
				}
			}
		},
		[](const EventBase&) { UNREACHABLE; }
	}, event);
	return false;
}

void InputEventGenerator::setGrabInput(bool grab) const
{
	SDL_SetWindowGrab(SDL_GL_GetCurrentWindow(), grab ? SDL_TRUE : SDL_FALSE);
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
