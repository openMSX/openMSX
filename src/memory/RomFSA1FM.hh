#ifndef ROMFSA1FM1_HH
#define ROMFSA1FM1_HH

#include "MSXRom.hh"
#include "RomBlocks.hh"
#include "FirmwareSwitch.hh"

namespace openmsx {

class SRAM;

class RomFSA1FM1 final : public MSXRom
{
public:
	RomFSA1FM1(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::shared_ptr<SRAM> fsSram;
	FirmwareSwitch firmwareSwitch;
};

class RomFSA1FM2 final : public Rom8kBBlocks
{
public:
	RomFSA1FM2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, byte bank);

private:
	std::shared_ptr<SRAM> fsSram;
	byte bankSelect[8];
	bool isRam[8];
	bool isEmpty[8];
	byte control;
};

} // namespace openmsx

#endif
