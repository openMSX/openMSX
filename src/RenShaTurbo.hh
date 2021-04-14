#ifndef RENSHATURBO_HH
#define RENSHATURBO_HH

#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CommandController;
class XMLElement;
class Autofire;

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
	RenShaTurbo(CommandController& commandController,
	            const XMLElement& machineConfig);
	~RenShaTurbo();

	/** Get the output signal in negative logic.
	  * @result When auto-fire is on, result will alternate between true
	  *         and false. When auto-fire if off result is false.
	  */
	[[nodiscard]] bool getSignal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// The Autofire circuit
	std::unique_ptr<Autofire> autofire;
};

} // namespace openmsx

#endif
