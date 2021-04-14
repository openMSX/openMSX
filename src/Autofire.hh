#ifndef AUTOFIRE_HH
#define AUTOFIRE_HH

#include "Observer.hh"
#include "DynamicClock.hh"
#include "EmuTime.hh"
#include "IntegerSetting.hh"
#include <string_view>

namespace openmsx {

class CommandController;

/**
 * Autofire is a device that is between two other devices and outside
 * the bus. For example, between the keyboard and the PPI
 * or between a joyport connecter and the PSG.
 *
 * There can be multiple autofire circuits. For example, one used
 * by the Ren-Sha Turbo and another one built into a joystick.
 */
class Autofire final : private Observer<Setting>
{
public:
	Autofire(CommandController& commandController,
	         unsigned newMinInts, unsigned newMaxInts,
	         std::string_view name);
	~Autofire();

	/** Get the output signal in negative logic.
	  * @result When auto-fire is on, result will alternate between true
	  *         and false. When auto-fire if off result is false.
	  */
	[[nodiscard]] bool getSignal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** Sets the clock frequency according to the current value of the speed
	  * settings.
	  */
	void setClock();

	void update(const Setting& setting) noexcept override;

private:
	// Following two values specify the range of the autofire
	// as measured by the test program:
	/** Number of interrupts at fastest setting (>=1).
	  * The number of interrupts for 50 periods, measured
	  * in ntsc mode (which gives 60 interrupts per second).
	  */
	const unsigned min_ints;
	/** Number of interrupts at slowest setting (>=min_ints+1).
	  * The number of interrupts for 50 periods, measured
	  * in ntsc mode (which gives 60 interrupts per second).
	  */
	const unsigned max_ints;

	/** The currently selected speed. */
	IntegerSetting speedSetting;

	/** Each tick of this clock, the signal changes.
	  * Frequency is derived from speed, min_ints and max_ints.
	  */
	DynamicClock clock;
};

} // namespace openmsx

#endif
