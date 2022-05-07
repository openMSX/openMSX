#ifndef ROMMANBOW2_HH
#define ROMMANBOW2_HH

#include "MSXRom.hh"
#include "RomTypes.hh"
#include "AmdFlash.hh"
#include "RomBlockDebuggable.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class AY8910;
class SCC;

class RomManbow2 final : public MSXRom
{
public:
	RomManbow2(const DeviceConfig& config, Rom&& rom, RomType type);
	~RomManbow2() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	void setRom(unsigned region, unsigned block);

private:
	const std::unique_ptr<SCC> scc;    // can be nullptr
	const std::unique_ptr<AY8910> psg; // can be nullptr
	AmdFlash flash;
	RomBlockDebuggable romBlockDebug;
	byte psgLatch;
	byte bank[4];
	bool sccEnabled;
};
SERIALIZE_CLASS_VERSION(RomManbow2, 3);

} // namespace openmsx

#endif
