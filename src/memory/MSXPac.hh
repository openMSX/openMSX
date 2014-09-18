#ifndef MSXPAC_HH
#define MSXPAC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;

class MSXPac final : public MSXDevice
{
public:
	explicit MSXPac(const DeviceConfig& config);
	virtual ~MSXPac();

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkSramEnable();

	const std::unique_ptr<SRAM> sram;
	byte r1ffe, r1fff;
	bool sramEnabled;
};

} // namespace openmsx

#endif
