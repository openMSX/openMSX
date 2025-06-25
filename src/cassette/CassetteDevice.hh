#ifndef CASSETTEDEVICE_HH
#define CASSETTEDEVICE_HH

#include "Pluggable.hh"
#include <cstdint>

namespace openmsx {

class CassetteDevice : public Pluggable
{
public:
	/**
	 * Sets the cassette motor relay
	 *  false = off   true = on
	 */
	virtual void setMotor(bool status, EmuTime time) = 0;

	/**
	 * Read wave data from cassette device
	 */
	virtual int16_t readSample(EmuTime time) = 0;

	/**
	 * Sets the cassette output signal
	 *  false = low   true = high
	 */
	virtual void setSignal(bool output, EmuTime time) = 0;

	// Pluggable
	[[nodiscard]] std::string_view getClass() const final;
};

} // namespace openmsx

#endif
