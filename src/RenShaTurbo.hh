// $Id$

#ifndef RENSHATURBO_HH
#define RENSHATURBO_HH

#include "openmsx.hh"
#include <memory>

namespace openmsx {

class CommandController;
class HardwareConfig;
class EmuTime;
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
	            HardwareConfig& hardwareConfig);
	~RenShaTurbo();

	/**
	 * Give the output signal in negative logic
	 *
	 * When auto-fire is on, bit 0 will
	 * alternate between 0 and 1
	 *
	 * When auto-fire is off, bit 0 will be 0
	 */
	byte getSignal(const EmuTime& time);

private:
	// The Autofire circuit
	std::auto_ptr<Autofire> autofire;
};

} // namespace openmsx

#endif
