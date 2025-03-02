#ifndef JOYSTICK_MANAGER_HH
#define JOYSTICK_MANAGER_HH

#include "JoystickId.hh"

#include <memory>
#include <optional>
#include <vector>

#include <SDL.h>

namespace openmsx {

class CommandController;
class IntegerSetting;

class JoystickManager {
public:
	JoystickManager(const JoystickManager&) = delete;
	JoystickManager(JoystickManager&&) = delete;
	JoystickManager& operator=(const JoystickManager&) = delete;
	JoystickManager& operator=(JoystickManager&&) = delete;

	explicit JoystickManager(CommandController& commandController_);
	~JoystickManager();

	// Handle SDL joystick added/removed events
	void add(int deviceIndex);
	void remove(int instanceId);

	// return the range of JoystickId's for which there is a corresponding SDL_Joystick
	[[nodiscard]] std::vector<JoystickId> getConnectedJoysticks() const;

	[[nodiscard]] IntegerSetting* getJoyDeadZoneSetting(JoystickId joyId) const;
	[[nodiscard]] std::string getDisplayName(JoystickId joyId) const;
	[[nodiscard]] std::optional<unsigned> getNumButtons(JoystickId joyId) const;

	[[nodiscard]] std::optional<JoystickId> translateSdlInstanceId(SDL_Event& evt) const;

private:
	[[nodiscard]] size_t getFreeSlot();

private:
	CommandController& commandController;

	struct Info {
		SDL_Joystick* joystick = nullptr;
		int instanceId = -1;
		std::unique_ptr<IntegerSetting> deadZoneSetting;
	};
	std::vector<Info> infos;
};

} // namespace openmsx

#endif
