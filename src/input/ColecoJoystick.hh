#ifndef COLECOJOYSTICK_HH
#define COLECOJOYSTICK_HH

#include "JoystickDevice.hh"

#include "BooleanInput.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "StringSetting.hh"

#include <array>
#include <string_view>
#include <vector>

namespace openmsx {

class CommandController;
class JoystickManager;
class MSXEventDistributor;
class StateChangeDistributor;

/** ColecoVision controller: a joystick with two fire buttons (L and R) and a
  * 12-key keypad. On an MSX joystick port it behaves like an MSX joystick
  * (L = button A, R = button B). ColecoJoystickIO additionally reads the
  * keypad via getKeypad().
  */
class ColecoJoystick final : public JoystickDevice, private MSXEventListener
                           , private StateChangeListener
{
public:
	ColecoJoystick(CommandController& commandController,
	               MSXEventDistributor& eventDistributor,
	               StateChangeDistributor& stateChangeDistributor,
	               JoystickManager& joystickManager,
	               uint8_t id);
	~ColecoJoystick() override;

	[[nodiscard]] static TclObject getDefaultConfig(JoystickId joyId, const JoystickManager& joystickManager);

	static constexpr unsigned NUM_BUTTONS = 18;
	// keys in the Tcl config dict, in the order of the 'status' bits
	static constexpr std::array<std::string_view, NUM_BUTTONS> keyNames = {
		"UP", "DOWN", "LEFT", "RIGHT", "L", "R",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "#",
	};

	/** Keypad state, active low: bit 0-9 = keys '0'-'9', bit 10 = '*',
	  * bit 11 = '#' (same layout as rows 2/3 of the 'cvjoy' key matrix).
	  */
	[[nodiscard]] unsigned getKeypad() const { return (status >> 6) & 0xFFF; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkJoystickConfig(const TclObject& newValue);

	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime time) noexcept override;

private:
	static constexpr unsigned ALL_RELEASED = (1u << NUM_BUTTONS) - 1;

	CommandController& commandController;
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	JoystickManager& joystickManager;
	StringSetting configSetting;

	// up, down, left, right, L, R, 0-9, *, # (in sync with 'status' bits)
	std::array<std::vector<BooleanInput>, NUM_BUTTONS> bindings; // calculated from 'configSetting'

	const std::string description;
	const uint8_t id;
	unsigned status = ALL_RELEASED; // active low
	bool pin8 = false;
};

} // namespace openmsx

#endif
