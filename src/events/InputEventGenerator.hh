#ifndef INPUTEVENTGENERATOR_HH
#define INPUTEVENTGENERATOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include "Keys.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class CommandController;
class BooleanSetting;
class EventDistributor;
class Setting;
class EscapeGrabCmd;

class InputEventGenerator final : private Observer<Setting>
                                , private EventListener
                                , private noncopyable
{
public:
	InputEventGenerator(CommandController& commandController,
	                    EventDistributor& eventDistributor);
	~InputEventGenerator();

	/** Wait for event(s) and handle it.
	  * This method should be called from the main thread.
	  */
	void wait();

	/**
	 * Enable or disable keyboard event repeats
	 */
	void setKeyRepeat(bool enable);

	/**
	 * This functions shouldn't be needed, but in the SDL library input
	 * and video or closely coupled (sigh). For example when the video mode
	 * is changed we need to reset the keyrepeat and unicode settings.
	 */
	void reinit();

	/** Input Grab on or off */
	BooleanSetting& getGrabInput() const { return *grabInput; }

	/** Normally the following two functions simply delegate to
	 * SDL_JoystickNumButtons() and SDL_JoystickGetButton(). Except on
	 * Android, see comments in .cc for more details.
	 */
	static int joystickNumButtons(SDL_Joystick* joystick);
	static bool joystickGetButton(SDL_Joystick* joystick, int button);

	void poll();

private:
	typedef std::shared_ptr<const Event> EventPtr;

	void handle(const SDL_Event& event);
	void setGrabInput(bool grab);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	EventDistributor& eventDistributor;
	const std::unique_ptr<BooleanSetting> grabInput;
	const std::unique_ptr<EscapeGrabCmd> escapeGrabCmd;
	friend class EscapeGrabCmd;

	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState;

	// OsdControl
	void setNewOsdControlButtonState(
		unsigned newState, const EventPtr& origEvent);
	void triggerOsdControlEventsFromJoystickAxisMotion(
		unsigned axis, short value, const EventPtr& origEvent);
	void triggerOsdControlEventsFromJoystickHat(
		int value, const EventPtr& origEvent);
	void osdControlChangeButton(
		bool up, unsigned changedButtonMask, const EventPtr& origEvent);
	void triggerOsdControlEventsFromJoystickButtonEvent(
		unsigned button, bool up, const EventPtr& origEvent);
	void triggerOsdControlEventsFromKeyEvent(
		Keys::KeyCode keyCode, bool up, const EventPtr& origEvent);


	bool keyRepeat;
	unsigned osdControlButtonsState; // 0 is pressed, 1 is released

	// only for Android
	static bool androidButtonA;
	static bool androidButtonB;
};

} // namespace openmsx

#endif
