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

#include "I8255.hh"
#include "I8255Interface.hh"
#include "MSXDevice.hh"
#include "Rom.hh"

#include <memory>

namespace openmsx {

class IDEDevice;

class BeerIDE final : public MSXDevice, public I8255Interface
{
public:
	explicit BeerIDE(DeviceConfig& config);
	~BeerIDE() override;

	void reset(EmuTime time) override;

	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;

	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeControl(uint8_t value, EmuTime time);

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
	I8255 i8255;
	Rom rom;
	std::unique_ptr<IDEDevice> device;
	uint16_t dataReg;
	uint8_t controlReg;
};

} // namespace openmsx

#endif
