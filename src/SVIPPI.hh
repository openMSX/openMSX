// This class implements the mapping of the PPI (8255) into the SVI machine.

#ifndef SVIPPI_HH
#define SVIPPI_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "Keyboard.hh"
#include "KeyClick.hh"
#include <array>

namespace openmsx {

class CassettePortInterface;
class JoystickPortIf;

class SVIPPI final : public MSXDevice, public I8255Interface
{
public:
	explicit SVIPPI(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	[[nodiscard]] byte readA(EmuTime::param time) override;
	[[nodiscard]] byte readB(EmuTime::param time) override;
	[[nodiscard]] nibble readC0(EmuTime::param time) override;
	[[nodiscard]] nibble readC1(EmuTime::param time) override;
	[[nodiscard]] byte peekA(EmuTime::param time) const override;
	[[nodiscard]] byte peekB(EmuTime::param time) const override;
	[[nodiscard]] nibble peekC0(EmuTime::param time) const override;
	[[nodiscard]] nibble peekC1(EmuTime::param time) const override;
	void writeA(byte value, EmuTime::param time) override;
	void writeB(byte value, EmuTime::param time) override;
	void writeC0(nibble value, EmuTime::param time) override;
	void writeC1(nibble value, EmuTime::param time) override;

private:
	CassettePortInterface& cassettePort;
	I8255 i8255;
	KeyClick click;
	Keyboard keyboard;
	std::array<JoystickPortIf*, 2> ports;
	nibble prevBits = 15;
	nibble selectedRow = 0;
};

} // namespace openmsx

#endif
