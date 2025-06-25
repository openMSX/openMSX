// This class implements the PPI (8255)
//
//   PPI    MSX-I/O  Direction  MSX-Function
//  PortA    0xA8      Out     Memory primary slot register
//  PortB    0xA9      In      Keyboard column inputs
//  PortC    0xAA      Out     Keyboard row select / CAPS / CASo / CASm / SND
//  Control  0xAB     In/Out   Mode select for PPI
//
//  Direction indicates the direction normally used on MSX.
//  Reading from an output port returns the last written byte.
//  Writing to an input port has no immediate effect.
//
//  PortA combined with upper half of PortC form groupA
//  PortB               lower                    groupB
//  GroupA can be in programmed in 3 modes
//   - basic input/output
//   - strobed input/output
//   - bidirectional
//  GroupB can only use the first two modes.
//  Only the first mode is used on MSX, only this mode is implemented yet.
//
//  for more detail see
//    http://w3.qahwah.net/joost/openMSX/8255.pdf

#ifndef MSXPPI_HH
#define MSXPPI_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "Keyboard.hh"
#include "KeyClick.hh"

namespace openmsx {

class CassettePortInterface;
class RenShaTurbo;

class MSXPPI final : public MSXDevice, public I8255Interface
{
public:
	explicit MSXPPI(const DeviceConfig& config);
	~MSXPPI() override;

	void reset(EmuTime time) override;
	void powerDown(EmuTime time) override;
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
	RenShaTurbo& renshaTurbo;
	I8255 i8255;
	KeyClick click;
	Keyboard keyboard;
	uint4_t prevBits = 15;
	uint4_t selectedRow = 0;
};

} // namespace openmsx

#endif
