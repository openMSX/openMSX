#ifndef PRINTERPORTDEVICE_HH
#define PRINTERPORTDEVICE_HH

#include "Pluggable.hh"
#include "openmsx.hh"

namespace openmsx {

class PrinterPortDevice : public Pluggable
{
public:
	/**
	 * Returns the STATUS signal:
	 *  false = low  = ready,
	 *  true  = high = not ready.
	 */
	[[nodiscard]] virtual bool getStatus(EmuTime::param time) = 0;

	/**
	 * Sets the strobe signal:
	 *  false = low,
	 *  true  = high.
	 * Normal high, a short pulse (low, high) means data is valid.
	 */
	virtual void setStrobe(bool strobe, EmuTime::param time) = 0;

	/**
	 * Sets the data signals.
	 * Always use strobe to see whether data is valid.
	 * As an optimization, this method might not be called when the
	 * new data is the same as the previous data.
	 */
	virtual void writeData(byte data, EmuTime::param time) = 0;

	// Pluggable
	[[nodiscard]] std::string_view getClass() const final;
};

} // namespace openmsx

#endif
