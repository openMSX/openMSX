// $Id$

#ifndef __MSXHBI55_HH__
#define __MSXHBI55_HH__

#include "MSXDevice.hh"
#include "I8255.hh"
#include <memory>

namespace openmsx {

class SRAM;

class MSXHBI55 : public MSXDevice, public I8255Interface
{
// MSXDevice
public:
	MSXHBI55(const XMLElement& config, const EmuTime& time); 
	virtual ~MSXHBI55(); 

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	const std::auto_ptr<I8255> i8255;

// I8255Interface
public:
	virtual byte readA(const EmuTime& time);
	virtual byte readB(const EmuTime& time);
	virtual nibble readC0(const EmuTime& time);
	virtual nibble readC1(const EmuTime& time);
	virtual byte peekA(const EmuTime& time) const;
	virtual byte peekB(const EmuTime& time) const;
	virtual nibble peekC0(const EmuTime& time) const;
	virtual nibble peekC1(const EmuTime& time) const;
	virtual void writeA(byte value, const EmuTime& time);
	virtual void writeB(byte value, const EmuTime& time);
	virtual void writeC0(nibble value, const EmuTime& time);
	virtual void writeC1(nibble value, const EmuTime& time);

private:
	byte readSRAM(word address) const;

	const std::auto_ptr<SRAM> sram;
	word readAddress;
	word writeAddress;
	byte addressLatch;
	byte writeLatch;
	byte mode;
};

} // namespace openmsx

#endif
