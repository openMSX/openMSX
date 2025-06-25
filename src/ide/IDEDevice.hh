#ifndef IDEDEVICE_HH
#define IDEDEVICE_HH

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

using uint4_t = uint8_t;

class IDEDevice
{
public:
	virtual ~IDEDevice() = default;
	virtual void reset(EmuTime time) = 0;

	[[nodiscard]] virtual uint16_t readData(EmuTime time) = 0;
	[[nodiscard]] virtual uint8_t readReg(uint4_t reg, EmuTime time) = 0;

	virtual void writeData(uint16_t value, EmuTime time) = 0;
	virtual void writeReg(uint4_t reg, uint8_t value, EmuTime time) = 0;
};

} // namespace openmsx

#endif
