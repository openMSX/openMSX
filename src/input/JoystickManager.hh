#ifndef JOYSTICK_MANAGER_HH
#define JOYSTICK_MANAGER_HH

#include "InfoTopic.hh"
#include "JoystickId.hh"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <SDL.h>

namespace openmsx {

class CommandController;
class JoystickAxisMotionEvent;
class FloatSetting;
class Reactor;

class JoystickManager {
public:
	struct Info {
		SDL_Joystick* joystick = nullptr;
		int instanceId = -1;
		std::unique_ptr<FloatSetting> deadZoneSetting;
		std::unique_ptr<FloatSetting> midPointSetting;
		std::unique_ptr<FloatSetting> saturationSetting;
	};
public:

	JoystickManager(const JoystickManager&) = delete;
	JoystickManager(JoystickManager&&) = delete;
	JoystickManager& operator=(const JoystickManager&) = delete;
	JoystickManager& operator=(JoystickManager&&) = delete;

	explicit JoystickManager(Reactor& reactor);
	~JoystickManager();

	// Handle SDL joystick added/removed events
	void add(int deviceIndex);
	void remove(int instanceId);

	// return the range of JoystickId's for which there is a corresponding SDL_Joystick
	[[nodiscard]] std::vector<JoystickId> getConnectedJoysticks() const;

	[[nodiscard]] int getJoyDeadThreshold(JoystickId joyId) const;
	[[nodiscard]] float getJoyValue(JoystickId joyId, const JoystickAxisMotionEvent& e) const;
	[[nodiscard]] Info* getSettings(JoystickId joyId);
	[[nodiscard]] std::string getDisplayName(JoystickId joyId) const;
	[[nodiscard]] std::optional<unsigned> getNumAxes(JoystickId joyId) const;
	[[nodiscard]] std::optional<unsigned> getNumBalls(JoystickId joyId) const;
	[[nodiscard]] std::optional<unsigned> getNumButtons(JoystickId joyId) const;
	[[nodiscard]] std::optional<unsigned> getNumHats(JoystickId joyId) const;
	[[nodiscard]] std::optional<int16_t> getAxis(JoystickId joyId, int axis) const;

	[[nodiscard]] std::optional<JoystickId> translateSdlInstanceId(SDL_Event& evt) const;

private:
	[[nodiscard]] size_t getFreeSlot();

private:
	CommandController& commandController;

	struct JoystickInfo final : InfoTopic {
		explicit JoystickInfo(InfoCommand& openMsxInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} joystickInfo;

	std::vector<Info> infos;
};

} // namespace openmsx

#endif
