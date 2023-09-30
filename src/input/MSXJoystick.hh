#ifndef MSXJOYSTICK_HH
#define MSXJOYSTICK_HH

#include "JoystickDevice.hh"

#include "BooleanInput.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

#include <vector>

namespace openmsx {

//class CommandController;
class GlobalSettings;
class MSXEventDistributor;
class StateChangeDistributor;

class MSXJoystick final : public JoystickDevice, private MSXEventListener
                        , private StateChangeListener
{
public:
	MSXJoystick(//CommandController& commandController,
	            MSXEventDistributor& eventDistributor,
	            StateChangeDistributor& stateChangeDistributor,
	            GlobalSettings& globalSettings,
	            uint8_t id);
	~MSXJoystick() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
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
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	GlobalSettings& globalSettings;

	std::vector<BooleanInput> up;
	std::vector<BooleanInput> down;
	std::vector<BooleanInput> left;
	std::vector<BooleanInput> right;
	std::vector<BooleanInput> trigA;
	std::vector<BooleanInput> trigB;

	const uint8_t id;
	uint8_t status;
	bool pin8;
};

} // namespace openmsx

#endif
