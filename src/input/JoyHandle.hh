#ifndef JOYHANDLE_HH
#define JOYHANDLE_HH

#include "JoystickDevice.hh"

#include "BooleanInput.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "StringSetting.hh"

#include <array>
#include <vector>

namespace openmsx {

class CommandController;
class JoystickManager;
class MSXEventDistributor;
class StateChangeDistributor;

class JoyHandle final : public JoystickDevice, private MSXEventListener
                        , private StateChangeListener
{
public:
	JoyHandle(CommandController& commandController,
	          MSXEventDistributor& eventDistributor,
	          StateChangeDistributor& stateChangeDistributor,
	          JoystickManager& joystickManager,
	          uint8_t id);
	~JoyHandle() override;

	[[nodiscard]] static TclObject getDefaultConfig(JoystickId joyId, const JoystickManager& joystickManager);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkJoystickConfig(const TclObject& newValue);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// MSXJoystickDevice
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

	void checkTime(EmuTime::param time);

private:
	CommandController& commandController;
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	JoystickManager& joystickManager;
	StringSetting configSetting;

	// up, down, left, right, a, b, wheel_left, wheel_right (in sync with order in JoystickDevice)
	std::array<std::vector<BooleanInput>, 8> bindings; // calculated from 'configSetting'

	const std::string description;
	const uint8_t id;
	uint8_t status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                 JOY_BUTTONA | JOY_BUTTONB;
	EmuTime lastTime = EmuTime::zero();
	uint8_t cycle = 0; // 0-1
	int8_t analogValue = 0;
};

[[nodiscard]] std::optional<int8_t> matchAnalog(const BooleanInput& binding, const Event& event,
												function_ref<int(JoystickId)> getJoyDeadZone);

} // namespace openmsx

#endif
