#ifndef I8255INTERFACE_HH
#define I8255INTERFACE_HH

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

using uint4_t = uint8_t;

class I8255Interface
{
public:
	[[nodiscard]] virtual uint8_t readA (EmuTime::param time) = 0;
	[[nodiscard]] virtual uint8_t readB (EmuTime::param time) = 0;
	[[nodiscard]] virtual uint4_t readC0(EmuTime::param time) = 0;
	[[nodiscard]] virtual uint4_t readC1(EmuTime::param time) = 0;
	[[nodiscard]] virtual uint8_t peekA (EmuTime::param time) const = 0;
	[[nodiscard]] virtual uint8_t peekB (EmuTime::param time) const = 0;
	[[nodiscard]] virtual uint4_t peekC0(EmuTime::param time) const = 0;
	[[nodiscard]] virtual uint4_t peekC1(EmuTime::param time) const = 0;
	virtual void writeA (uint8_t value, EmuTime::param time) = 0;
	virtual void writeB (uint8_t value, EmuTime::param time) = 0;
	virtual void writeC0(uint4_t value, EmuTime::param time) = 0;
	virtual void writeC1(uint4_t value, EmuTime::param time) = 0;

protected:
	~I8255Interface() = default;
};

} // namespace openmsx

#endif
