// $Id$

#ifndef __MSXMUSIC_HH__
#define __MSXMUSIC_HH__

#include <memory>
#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class YM2413Core;

class MSXMusic : public MSXDevice
{
public:
	MSXMusic(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMusic();
	
	virtual void reset(const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	void writeRegisterPort(byte value, const EmuTime& time);
	void writeDataPort(byte value, const EmuTime& time);

	Rom rom;
	std::auto_ptr<YM2413Core> ym2413;

private:
	int registerLatch;
};

} // namespace openmsx

#endif
