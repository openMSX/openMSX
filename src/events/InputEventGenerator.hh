#ifndef INPUTEVENTGENERATOR_HH
#define INPUTEVENTGENERATOR_HH

#include "BooleanSetting.hh"
#include "Command.hh"
#include "EventListener.hh"
#include "JoystickManager.hh"

#include "SDLKey.hh"
#include <SDL.h>

namespace openmsx {

class CommandController;
class EventDistributor;
class GlobalSettings;

class InputEventGenerator final : private EventListener
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

	/** Input Grab on or off */
	[[nodiscard]] BooleanSetting& getGrabInput() { return grabInput; }
	/** Must be called when 'grabinput' or 'fullscreen' setting changes. */
	void updateGrab(bool grab);

	void poll();

	[[nodiscard]] JoystickManager& getJoystickManager() { return joystickManager; }

private:
	void handle(const SDL_Event& evt);
	void handleKeyDown(const SDL_KeyboardEvent& key, uint32_t unicode);
	void splitText(uint32_t timestamp, const char* utf8);
	void setGrabInput(bool grab);

	// EventListener
	int signalEvent(const Event& event) override;

	EventDistributor& eventDistributor;
	GlobalSettings& globalSettings;
	JoystickManager joystickManager;
	BooleanSetting grabInput;

	struct EscapeGrabCmd final : Command {
		explicit EscapeGrabCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} escapeGrabCmd;

	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState = ESCAPE_GRAB_WAIT_CMD;

	// OsdControl
	void setNewOsdControlButtonState(unsigned newState);
	void triggerOsdControlEventsFromJoystickAxisMotion(unsigned axis, int value);
	void triggerOsdControlEventsFromJoystickHat(int value);
	void osdControlChangeButton(bool down, unsigned changedButtonMask);
	void triggerOsdControlEventsFromJoystickButtonEvent(unsigned button, bool down);
	void triggerOsdControlEventsFromKeyEvent(SDLKey key, bool repeat);

private:
	unsigned osdControlButtonsState = unsigned(~0); // 0 is pressed, 1 is released

	// only for Android
	static inline bool androidButtonA = false;
	static inline bool androidButtonB = false;
};

} // namespace openmsx

#endif
