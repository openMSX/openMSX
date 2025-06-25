#ifndef PRINTERPORTDEVICE_HH
#define PRINTERPORTDEVICE_HH

#include "Pluggable.hh"
#include <cstdint>

namespace openmsx {

class PrinterPortDevice : public Pluggable
{
public:
	/**
	 * Returns the STATUS signal:
	 *  false = low  = ready,
	 *  true  = high = not ready.
	 */
	[[nodiscard]] virtual bool getStatus(EmuTime time) = 0;

	/**
	 * Sets the strobe signal:
	 *  false = low,
	 *  true  = high.
	 * Normal high, a short pulse (low, high) means data is valid.
	 */
	virtual void setStrobe(bool strobe, EmuTime time) = 0;

	/**
	 * Sets the data signals.
	 * Always use strobe to see whether data is valid.
	 * As an optimization, this method might not be called when the
	 * new data is the same as the previous data.
	 */
	virtual void writeData(uint8_t data, EmuTime time) = 0;

	// Pluggable
	[[nodiscard]] std::string_view getClass() const final;
};

} // namespace openmsx

#endif
