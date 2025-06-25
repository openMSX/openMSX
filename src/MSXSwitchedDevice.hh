#ifndef MSXSWITCHEDDEVICE_HH
#define MSXSWITCHEDDEVICE_HH

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

class MSXMotherBoard;

class MSXSwitchedDevice
{
public:
	[[nodiscard]] virtual uint8_t readSwitchedIO(uint16_t port, EmuTime time) = 0;
	[[nodiscard]] virtual uint8_t peekSwitchedIO(uint16_t port, EmuTime time) const = 0;
	virtual void writeSwitchedIO(uint16_t port, uint8_t value, EmuTime time) = 0;

protected:
	MSXSwitchedDevice(MSXMotherBoard& motherBoard, uint8_t id);
	~MSXSwitchedDevice();

private:
	MSXMotherBoard& motherBoard;
	const uint8_t id;
};

} // namespace openmsx

#endif
