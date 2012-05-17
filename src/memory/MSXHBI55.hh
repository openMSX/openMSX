// $Id$

#ifndef MSXHBI55_HH
#define MSXHBI55_HH

#include "MSXDevice.hh"
#include "I8255Interface.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

class I8255;
class SRAM;

class MSXHBI55 : public MSXDevice, private I8255Interface
{
// MSXDevice
public:
	explicit MSXHBI55(const DeviceConfig& config);
	virtual ~MSXHBI55();

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// I8255Interface
	virtual byte readA(EmuTime::param time);
	virtual byte readB(EmuTime::param time);
	virtual nibble readC0(EmuTime::param time);
	virtual nibble readC1(EmuTime::param time);
	virtual byte peekA(EmuTime::param time) const;
	virtual byte peekB(EmuTime::param time) const;
	virtual nibble peekC0(EmuTime::param time) const;
	virtual nibble peekC1(EmuTime::param time) const;
	virtual void writeA(byte value, EmuTime::param time);
	virtual void writeB(byte value, EmuTime::param time);
	virtual void writeC0(nibble value, EmuTime::param time);
	virtual void writeC1(nibble value, EmuTime::param time);

	byte readSRAM(word address) const;

	const std::auto_ptr<I8255> i8255;
	const std::auto_ptr<SRAM> sram;
	word readAddress;
	word writeAddress;
	byte addressLatch;
	byte writeLatch;
	byte mode;
};

} // namespace openmsx

#endif
