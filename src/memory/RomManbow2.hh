#ifndef ROMMANBOW2_HH
#define ROMMANBOW2_HH

#include "MSXRom.hh"
#include "RomTypes.hh"
#include "AmdFlash.hh"
#include "RomBlockDebuggable.hh"
#include "serialize_meta.hh"
#include <array>
#include <memory>

namespace openmsx {

class AY8910;
class SCC;

class RomManbow2 final : public MSXRom
{
public:
	RomManbow2(DeviceConfig& config, Rom&& rom, RomType type);
	~RomManbow2() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRom(unsigned region, byte block);

private:
	const std::unique_ptr<SCC> scc;    // can be nullptr
	const std::unique_ptr<AY8910> psg; // can be nullptr
	AmdFlash flash;
	RomBlockDebuggable romBlockDebug;
	byte psgLatch;
	std::array<byte, 4> bank;
	bool sccEnabled;
};
SERIALIZE_CLASS_VERSION(RomManbow2, 3);

} // namespace openmsx

#endif
