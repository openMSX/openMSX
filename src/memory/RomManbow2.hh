#ifndef ROMMANBOW2_HH
#define ROMMANBOW2_HH

#include "MSXRom.hh"
#include "RomTypes.hh"
#include "SCC.hh"
#include "AmdFlash.hh"
#include "RomBlockDebuggable.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class AY8910;

class RomManbow2 final : public MSXRom
{
public:
	RomManbow2(const DeviceConfig& config, Rom&& rom, RomType type);
	~RomManbow2();

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRom(unsigned region, unsigned block);

	SCC scc;
	const std::unique_ptr<AY8910> psg; // can be nullptr
	AmdFlash flash;
	RomBlockDebuggable romBlockDebug;
	byte psgLatch;
	byte bank[4];
	bool sccEnabled;
};
SERIALIZE_CLASS_VERSION(RomManbow2, 2);

} // namespace openmsx

#endif
