#ifndef RENSHATURBO_HH
#define RENSHATURBO_HH

#include "Autofire.hh"
#include "EmuTime.hh"
#include <optional>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;

/**
 * Ren-Sha Turbo is the autofire in several MSX 2+ models and in
 * the MSX turbo R. It works on space bar and on fire-button 1 of
 * both joystick ports
 *
 * It uses one autofire circuit.
 */
class RenShaTurbo
{
public:
	RenShaTurbo(MSXMotherBoard& motherBoard,
	            const XMLElement& machineConfig);

	/** Get the output signal in negative logic.
	  * @result When auto-fire is on, result will alternate between true
	  *         and false. When auto-fire if off result is false.
	  */
	[[nodiscard]] bool getSignal(EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// The Autofire circuit
	std::optional<Autofire> autofire;
};

} // namespace openmsx

#endif
