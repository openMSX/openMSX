// $Id$

#ifndef __RENSHATURBO_HH__
#define __RENSHATURBO_HH__

#include <memory>
#include "openmsx.hh"

using std::auto_ptr;

namespace openmsx {

class EmuTime;
class Autofire;
class SettingsConfig;

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
	static RenShaTurbo& instance();

	/**
	 * Give the output signal in negative logic
	 *
	 * When auto-fire is on, bit 0 will
	 * alternate between 0 and 1
	 *
	 * When auto-fire is off, bit 0 will be 0
	 */
	byte getSignal(const EmuTime &time);

private:
	RenShaTurbo();
	~RenShaTurbo();

	// The Autofire circuit
	auto_ptr<Autofire> autofire;
};

} // namespace openmsx

#endif
