// $Id$

#ifndef MSXMEGARAM_HH
#define MSXMEGARAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Ram;
class Rom;
class RomBlockDebuggable;

class MSXMegaRam : public MSXDevice
{
public:
	explicit MSXMegaRam(const DeviceConfig& config);
	virtual ~MSXMegaRam();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte page, byte block);

	const unsigned numBlocks; // must come before ram
	const std::unique_ptr<Ram> ram;
	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	const byte maskBlocks;
	byte bank[4];
	bool writeMode;
	bool romMode;
};

} // namespace openmsx

#endif
