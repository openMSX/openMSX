#ifndef INPUTEVENTGENERATOR_HH
#define INPUTEVENTGENERATOR_HH

#include "Observer.hh"
#include "BooleanSetting.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "Keys.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalSettings;

class InputEventGenerator final : private Observer<Setting>
                                , private EventListener
{
public:
	InputEventGenerator(const InputEventGenerator&) = delete;
	InputEventGenerator& operator=(const InputEventGenerator&) = delete;

	InputEventGenerator(CommandController& commandController,
	                    EventDistributor& eventDistributor,
	                    GlobalSettings& globalSettings);
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
	BooleanSetting& getGrabInput() { return grabInput; }

	/** Normally the following two functions simply delegate to
	 * SDL_JoystickNumButtons() and SDL_JoystickGetButton(). Except on
	 * Android, see comments in .cc for more details.
	 */
	static int joystickNumButtons(SDL_Joystick* joystick);
	static bool joystickGetButton(SDL_Joystick* joystick, int button);

	void poll();

private:
	using EventPtr = std::shared_ptr<const Event>;

	void handle(const SDL_Event& evt);
	void setGrabInput(bool grab);

	// Observer<Setting>
	void update(const Setting& setting) override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	EventDistributor& eventDistributor;
	GlobalSettings& globalSettings;
	BooleanSetting grabInput;

	struct EscapeGrabCmd final : Command {
		explicit EscapeGrabCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} escapeGrabCmd;

	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState;

	// OsdControl
	void setNewOsdControlButtonState(
		unsigned newState, const EventPtr& origEvent);
	void triggerOsdControlEventsFromJoystickAxisMotion(
		unsigned axis, int value, const EventPtr& origEvent);
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
