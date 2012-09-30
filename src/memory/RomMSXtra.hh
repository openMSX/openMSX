//

#ifndef ROMMSXTRA_HH
#define ROMMSXTRA_HH

#include "MSXRom.hh"

namespace openmsx {

class Ram;

class RomMSXtra : public MSXRom
{
public:
	RomMSXtra(const DeviceConfig& config, std::auto_ptr<Rom> rom);
	virtual ~RomMSXtra();

	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	const std::auto_ptr<Ram> ram;
};

} // namespace

#endif
