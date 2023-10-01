#ifndef MSXJOYSTICK_HH
#define MSXJOYSTICK_HH

#include "JoystickDevice.hh"

#include "BooleanInput.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "StringSetting.hh"

#include <array>
#include <vector>

namespace openmsx {

class CommandController;
class GlobalSettings;
class MSXEventDistributor;
class StateChangeDistributor;

class MSXJoystick final : public JoystickDevice, private MSXEventListener
                        , private StateChangeListener
{
public:
	MSXJoystick(CommandController& commandController,
	            MSXEventDistributor& eventDistributor,
	            StateChangeDistributor& stateChangeDistributor,
	            GlobalSettings& globalSettings,
	            uint8_t id);
	~MSXJoystick() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkJoystickConfig(TclObject& newValue);

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

private:
	CommandController& commandController;
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	GlobalSettings& globalSettings;
	StringSetting configSetting;

	// up, down, left, right, a, b (in sync with order in JoystickDevice)
	std::array<std::vector<BooleanInput>, 6> bindings; // calculated from 'configSetting'

	const uint8_t id;
	uint8_t status;
	bool pin8;
};

} // namespace openmsx

#endif
