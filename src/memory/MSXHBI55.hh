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
	MSXHBI55(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXHBI55();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<I8255> i8255;

	// I8255Interface
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

	byte readSRAM(word address) const;

	const std::auto_ptr<SRAM> sram;
	word readAddress;
	word writeAddress;
	byte addressLatch;
	byte writeLatch;
	byte mode;
};

REGISTER_MSXDEVICE(MSXHBI55, "MSXHBI55");

} // namespace openmsx

#endif
