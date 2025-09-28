// This class implements the mapping of the PPI (8255) into the SVI machine.

#ifndef SVIPPI_HH
#define SVIPPI_HH

#include "I8255.hh"
#include "I8255Interface.hh"
#include "KeyClick.hh"
#include "Keyboard.hh"
#include "MSXDevice.hh"

#include <array>

namespace openmsx {

class CassettePortInterface;
class JoystickPortIf;

class SVIPPI final : public MSXDevice, public I8255Interface
{
public:
	explicit SVIPPI(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	[[nodiscard]] uint8_t readA (EmuTime time) override;
	[[nodiscard]] uint8_t readB (EmuTime time) override;
	[[nodiscard]] uint4_t readC0(EmuTime time) override;
	[[nodiscard]] uint4_t readC1(EmuTime time) override;
	[[nodiscard]] uint8_t peekA (EmuTime time) const override;
	[[nodiscard]] uint8_t peekB (EmuTime time) const override;
	[[nodiscard]] uint4_t peekC0(EmuTime time) const override;
	[[nodiscard]] uint4_t peekC1(EmuTime time) const override;
	void writeA (uint8_t value, EmuTime time) override;
	void writeB (uint8_t value, EmuTime time) override;
	void writeC0(uint4_t value, EmuTime time) override;
	void writeC1(uint4_t value, EmuTime time) override;

private:
	CassettePortInterface& cassettePort;
	I8255 i8255;
	KeyClick click;
	Keyboard keyboard;
	std::array<JoystickPortIf*, 2> ports;
	uint4_t prevBits = 15;
	uint4_t selectedRow = 0;
};

} // namespace openmsx

#endif
