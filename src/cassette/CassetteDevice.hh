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
	virtual void setMotor(bool status, EmuTime::param time) = 0;

	/**
	 * Get the cassette input bit.
	 *
	 * In reality this is an analog signal (and in the past his method
	 * returned a int16_t wave signal). However we shift the responsibility
	 * of filtering and thresholding this signal from the emulated MSX to
	 * the cassette image implementation (because that make the Schmitt
	 * trigger easier to implement).
	 */
	virtual bool cassetteIn(EmuTime::param time) = 0;

	/**
	 * Sets the cassette output signal
	 *  false = low   true = high
	 */
	virtual void setSignal(bool output, EmuTime::param time) = 0;

	// Pluggable
	string_ref getClass() const final override;
};

} // namespace openmsx

#endif
