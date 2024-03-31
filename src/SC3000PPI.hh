#ifndef SC3000PPI_HH
#define SC3000PPI_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "Keyboard.hh"
#include <array>

namespace openmsx {

class CassettePortInterface;
class JoystickPortIf;

/** Connects SC-3000 peripherals to the PPI (8255).
  */
class SC3000PPI final : public MSXDevice, public I8255Interface
{
public:
	explicit SC3000PPI(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	byte readA(EmuTime::param time) override;
	byte readB(EmuTime::param time) override;
	nibble readC0(EmuTime::param time) override;
	nibble readC1(EmuTime::param time) override;
	byte peekA(EmuTime::param time) const override;
	byte peekB(EmuTime::param time) const override;
	nibble peekC0(EmuTime::param time) const override;
	nibble peekC1(EmuTime::param time) const override;
	void writeA(byte value, EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;
	void writeC0(nibble value, EmuTime::param time) override;
	void writeC1(nibble value, EmuTime::param time) override;

	CassettePortInterface& cassettePort;
	I8255 i8255;
	Keyboard keyboard;
	std::array<JoystickPortIf*, 2> ports;
	nibble prevBits = 15;
	nibble selectedRow = 0;
};

} // namespace openmsx

#endif
