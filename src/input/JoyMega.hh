#ifndef JOYMEGA_HH
#define JOYMEGA_HH

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

class JoyMega final : public JoystickDevice, private MSXEventListener
                    , private StateChangeListener
{
public:
	JoyMega(CommandController& commandController,
	        MSXEventDistributor& eventDistributor,
	        StateChangeDistributor& stateChangeDistributor,
	        GlobalSettings& globalSettings,
	        uint8_t id);
	~JoyMega() override ;

	[[nodiscard]] static TclObject getDefaultConfig(uint8_t id);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkJoystickConfig(TclObject& newValue);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime::param time) override;
	void write(uint8_t value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

	void plugHelper2();
	void checkTime(EmuTime::param time);

private:
	CommandController& commandController;
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	GlobalSettings& globalSettings;
	StringSetting configSetting;

	// 0...3 :  up, down, left, right
	// 4...7 :  a, b, c, start
	// 8..11 :  x, y, z, select
	std::array<std::vector<BooleanInput>, 12> bindings; // calculated from 'configSetting'

	const std::string description;
	EmuTime lastTime = EmuTime::zero();
	unsigned status = 0xfff;
	uint8_t cycle; // 0-7
	uint8_t cycleMask; // 1 or 7
	const uint8_t id;
};

} // namespace openmsx

#endif
