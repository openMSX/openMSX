#ifndef MSXHBI55_HH
#define MSXHBI55_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "I8255.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXHBI55 final : public MSXDevice, public I8255Interface
{
public:
	explicit MSXHBI55(const DeviceConfig& config);

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

	byte readSRAM(word address) const;

	I8255 i8255;
	SRAM sram;
	word readAddress;
	word writeAddress;
	byte addressLatch;
	byte writeLatch;
	byte mode;
};

} // namespace openmsx

#endif
