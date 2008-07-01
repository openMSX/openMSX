// $Id$

#ifndef MSXFMPAC_HH
#define MSXFMPAC_HH

#include "MSXMusic.hh"
#include <memory>

namespace openmsx {

class SRAM;

class MSXFmPac : public MSXMusic
{
public:
	MSXFmPac(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXFmPac();

	virtual void reset(const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkSramEnable();

	const std::auto_ptr<SRAM> sram;
	byte enable;
	byte bank;
	byte r1ffe, r1fff;
	bool sramEnabled;
};
REGISTER_MSXDEVICE(MSXFmPac, "FM-PAC");

} // namespace openmsx

#endif
