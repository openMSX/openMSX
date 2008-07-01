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

	virtual void reset(const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	void writeRegisterPort(byte value, const EmuTime& time);
	void writeDataPort(byte value, const EmuTime& time);

	const std::auto_ptr<Rom> rom;
	std::auto_ptr<YM2413Interface> ym2413;

private:
	int registerLatch;
};

REGISTER_MSXDEVICE(MSXMusic, "MSX-Music");

} // namespace openmsx

#endif
