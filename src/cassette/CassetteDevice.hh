// $Id$

#ifndef CASSETTEDEVICE_HH
#define CASSETTEDEVICE_HH

#include "Pluggable.hh"

namespace openmsx {

class CassetteDevice : public Pluggable
{
public:
	/**
	 * Sets the cassette motor relay
	 *  false = off   true = on
	 */
	virtual void setMotor(bool status, const EmuTime &time) = 0;

	/**
	 * Read wave data from cassette device
	 */
	virtual short readSample(const EmuTime &time) = 0;

	/**
	 * Sets the cassette output signal
	 *  false = low   true = high
	 */
	virtual void setSignal(bool output, const EmuTime &time) = 0;

	// Pluggable
	virtual const std::string &getClass() const;
};

} // namespace openmsx

#endif
