// $Id$

#ifndef RENSHATURBO_HH
#define RENSHATURBO_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class XMLElement;
class EmuTime;
class Autofire;

/**
 * Ren-Sha Turbo is the autofire in several MSX 2+ models and in
 * the MSX turbo R. It works on space bar and on fire-button 1 of
 * both joystick ports
 *
 * It uses one autofire circuit.
 */
class RenShaTurbo : private noncopyable
{
public:
	RenShaTurbo(CommandController& commandController,
	            const XMLElement& machineConfig);
	~RenShaTurbo();

	/** Get the output signal in negative logic.
	  * @result When auto-fire is on, result will alternate between true
	  *         and false. When auto-fire if off result is false.
	  */
	bool getSignal(const EmuTime& time);

private:
	// The Autofire circuit
	std::auto_ptr<Autofire> autofire;
};

} // namespace openmsx

#endif
