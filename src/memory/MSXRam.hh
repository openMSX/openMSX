// $Id$

#ifndef MSXRAM_HH
#define MSXRAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class CheckedRam;

class MSXRam : public MSXDevice
{
public:
	explicit MSXRam(const DeviceConfig& config);

	virtual void powerUp(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	virtual byte peekMem(word address, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	virtual void init();
	inline unsigned translate(unsigned address) const;

	/*const*/ unsigned base;
	/*const*/ unsigned size;
	/*const*/ std::auto_ptr<CheckedRam> checkedRam;
};

} // namespace openmsx

#endif
