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

	void reset(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;
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
	RenShaTurbo& renshaTurbo;
	I8255 i8255;
	KeyClick click;
	Keyboard keyboard;
	nibble prevBits = 15;
	nibble selectedRow = 0;
};

} // namespace openmsx

#endif
