#ifndef YAMANOOTO_HH
#define YAMANOOTO_HH

#include "AY8910.hh"
#include "AmdFlash.hh"
#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"
#include "SCC.hh"

#include <array>

namespace openmsx {

class Yamanooto final : public MSXRom
{
public:
	Yamanooto(DeviceConfig& config, Rom&& rom);
	~Yamanooto() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	byte peekIO(uint16_t port, EmuTime time) const override;
	byte readIO(uint16_t port, EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeConfigReg(byte value);
	[[nodiscard]] bool isSCCAccess(uint16_t addr) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(const Yamanooto& device)
			: RomBlockDebuggableBase(device) {}
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} romBlockDebug;

	AmdFlash flash;
	SCC scc;
	AY8910 psg;
	std::array<uint16_t, 4> bankRegs = {}; // need 10 bits per entry (not 8)
	std::array<uint8_t, 4> rawBanks = {}; // not adjusted with offset
	byte enableReg = 0;
	byte offsetReg = 0;
	byte configReg = 0;
	byte sccMode = 0;
	byte psgLatch = 0;
	byte fpgaFsm = 0; // hack, just enough to read ID
};
SERIALIZE_CLASS_VERSION(Yamanooto, 2);

} // namespace openmsx

#endif
