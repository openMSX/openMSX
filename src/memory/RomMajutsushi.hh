#ifndef ROMMAJUTSUSHI_HH
#define ROMMAJUTSUSHI_HH

#include "RomKonami.hh"

namespace openmsx {

class DACSound8U;

class RomMajutsushi : public RomKonami
{
public:
	RomMajutsushi(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~RomMajutsushi();

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
