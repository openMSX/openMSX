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
	uint8_t readIO(uint16_t port, EmuTime::param time) override;
	uint8_t peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	uint8_t readA (EmuTime::param time) override;
	uint8_t readB (EmuTime::param time) override;
	uint4_t readC0(EmuTime::param time) override;
	uint4_t readC1(EmuTime::param time) override;
	uint8_t peekA (EmuTime::param time) const override;
	uint8_t peekB (EmuTime::param time) const override;
	uint4_t peekC0(EmuTime::param time) const override;
	uint4_t peekC1(EmuTime::param time) const override;
	void writeA (uint8_t value, EmuTime::param time) override;
	void writeB (uint8_t value, EmuTime::param time) override;
	void writeC0(uint4_t value, EmuTime::param time) override;
	void writeC1(uint4_t value, EmuTime::param time) override;

	CassettePortInterface& cassettePort;
	I8255 i8255;
	Keyboard keyboard;
	std::array<JoystickPortIf*, 2> ports;
	uint4_t prevBits = 15;
	uint4_t selectedRow = 0;
};

} // namespace openmsx

#endif
