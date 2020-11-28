#ifndef BEERIDE_HH
#define BEERIDE_HH

// Based on the blueMSX implementation:
//  Source: /cygdrive/d/Private/_SVNROOT/bluemsx/blueMSX/Src/Memory/romMapperBeerIDE.c,v Revision: 1.9 Date: 2008-03-31 19:42:22

/*
PPI    NAME   IDE PIN
---    ----   -------
PA0    HD0    17 D0
PA1    HD1    15 D1
PA2    HD2    13 D2
PA3    HD3    11 D3
PA4    HD4     9 D4
PA5    HD5     7 D5
PA6    HD6     5 D6
PA7    HD7     3 D7

PB0    HD8     4 D8
PB1    HD9     6 D9
PB2    HD10    8 D10
PB3    HD11   10 D11
PB4    HD12   12 D12
PB5    HD13   14 D13
PB6    HD14   16 D14
PB7    HD15   18 D15

PC0    HA0    35 A0
PC1    HA1    33 A1
PC2    HA2    36 A2
PC3    N/A
PC4    N/A
PC5    HCS    37 /CS0
PC6    HWR    23 /IOWR
PC7    HRD    25 /IORD
*/


#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "Rom.hh"
#include <memory>

namespace openmsx {

class IDEDevice;

class BeerIDE final : public MSXDevice, public I8255Interface
{
public:
	explicit BeerIDE(const DeviceConfig& config);
	~BeerIDE() override;

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;

	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeControl(byte value, EmuTime::param time);

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
	I8255 i8255;
	Rom rom;
	std::unique_ptr<IDEDevice> device;
	word dataReg;
	byte controlReg;
};

} // namespace openmsx

#endif
