// $Id$

#ifndef __MSXHBI55_HH__
#define __MSXHBI55_HH__

#include <memory>
#include "MSXIODevice.hh"
#include "I8255.hh"
#include "SRAM.hh"

using std::auto_ptr;

namespace openmsx {

class MSXHBI55 : public MSXIODevice, public I8255Interface
{
// MSXDevice
public:
	MSXHBI55(const XMLElement& config, const EmuTime& time); 
	virtual ~MSXHBI55(); 

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	auto_ptr<I8255> i8255;

// I8255Interface
public:
	virtual byte readA(const EmuTime& time);
	virtual byte readB(const EmuTime& time);
	virtual nibble readC0(const EmuTime& time);
	virtual nibble readC1(const EmuTime& time);
	virtual void writeA(byte value, const EmuTime& time);
	virtual void writeB(byte value, const EmuTime& time);
	virtual void writeC0(nibble value, const EmuTime& time);
	virtual void writeC1(nibble value, const EmuTime& time);

private:
	byte readSRAM(word address);
	SRAM sram;
	word address;
	byte mode;
};

} // namespace openmsx

#endif
