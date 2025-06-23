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

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	byte peekIO(uint16_t port, EmuTime::param time) const override;
	byte readIO(uint16_t port, EmuTime::param time) override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

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
	byte enableReg = 0;
	byte offsetReg = 0;
	byte configReg = 0;
	byte sccMode = 0;
	byte psgLatch = 0;
	byte fpgaFsm = 0; // hack, just enough to read ID
};

} // namespace openmsx

#endif
