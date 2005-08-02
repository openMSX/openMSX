// $Id$

#ifndef AY8910PERIPHERY_HH
#define AY8910PERIPHERY_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

/** Models the general purpose I/O ports of the AY8910.
  * The default implementation handles an empty periphery:
  * nothing is connected to the I/O ports.
  * This class can be overridden to connect peripherals.
  */
class AY8910Periphery
{
public:
	/** Reads the state of the peripheral on port A.
	  * Since the AY8910 doesn't have control lines for the I/O ports,
	  * a peripheral is not aware that it is read, which means that
	  * "peek" and "read" are equivalent.
	  * @param time The moment in time the peripheral's state is read.
	  *   On subsequent calls, the time will always be increasing.
	  * @return the value read; unconnected bits should be 1
	  */
	virtual byte readA(const EmuTime& time);

	/** Similar to readA, but reads port B. */
	virtual byte readB(const EmuTime& time);

	/** Writes to the peripheral on port A.
	  * @param value The value to write.
	  * @param time The moment in time the value is written.
	  *   On subsequent calls, the time will always be increasing.
	  */
	virtual void writeA(byte value, const EmuTime& time);

	/** Similar to writeA, but writes port B. */
	virtual void writeB(byte value, const EmuTime& time);

protected:
	AY8910Periphery();
	virtual ~AY8910Periphery();
};

} // namespace openmsx

#endif // AY8910PERIPHERY_HH
