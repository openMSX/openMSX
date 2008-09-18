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
	MSXMegaRam(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXMegaRam();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte page, byte block);

	const unsigned numBlocks; // must come before ram
	const std::auto_ptr<Ram> ram;
	const std::auto_ptr<Rom> rom;
	const byte maskBlocks;
	byte bank[4];
	bool writeMode;
	bool romMode;
};

} // namespace openmsx

#endif
