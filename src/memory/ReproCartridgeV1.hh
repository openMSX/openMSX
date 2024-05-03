#ifndef REPROCARTRIDGEV1_HH
#define REPROCARTRIDGEV1_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "AY8910.hh"
#include <array>

namespace openmsx {

class ReproCartridgeV1 final : public MSXRom
{
public:
	ReproCartridgeV1(const DeviceConfig& config, Rom&& rom);
	~ReproCartridgeV1() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	void writeIO(word port, byte value, EmuTime::param time) override;

	void setVolume(EmuTime::param time, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isSCCAccess(word addr) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

private:
	AmdFlash flash;
	SCC scc;
	AY8910 psg;

	bool flashRomWriteEnabled;
	byte mainBankReg;
	byte psgLatch;
	byte sccMode;
	std::array<byte, 4> bankRegs;
};

} // namespace openmsx

#endif
