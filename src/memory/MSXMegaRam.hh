// $Id$

#ifndef MSXMEGARAM_HH
#define MSXMEGARAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Ram;
class Rom;

class MSXMegaRam : public MSXDevice
{
public:
	MSXMegaRam(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMegaRam();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	void setBank(byte page, byte block);

	std::auto_ptr<Ram> ram;
	std::auto_ptr<Rom> rom;
	byte numBlocks;
	byte maskBlocks;
	byte bank[4];
	bool writeMode;
	bool romMode;
};

} // namespace openmsx

#endif
