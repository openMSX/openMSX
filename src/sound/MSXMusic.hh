// $Id$

#ifndef MSXMUSIC_HH
#define MSXMUSIC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;
class YM2413Interface;

class MSXMusic : public MSXDevice
{
public:
	MSXMusic(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXMusic();

	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void writeRegisterPort(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);

	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<YM2413Interface> ym2413;

private:
	int registerLatch;
};

} // namespace openmsx

#endif
