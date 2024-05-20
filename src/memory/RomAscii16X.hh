#ifndef ROMASCII16X_HH
#define ROMASCII16X_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SimpleDebuggable.hh"
#include <array>

namespace openmsx {

class RomAscii16X final : public MSXRom
{
public:
	explicit RomAscii16X(const DeviceConfig& config);
	RomAscii16X(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getFlashAddr(word addr) const;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	AmdFlash flash;

	std::array<word, 2> bankRegs = {0, 0};
};

} // namespace openmsx

#endif
