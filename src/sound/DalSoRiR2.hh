#ifndef DALSORIR2_HH
#define DALSORIR2_HH

#include "AmdFlash.hh"
#include "MSXDevice.hh"
#include "Observer.hh"
#include "YMF278B.hh"

namespace openmsx {

class DalSoRiR2 final : public MSXDevice, private Observer<Setting>
{
public:
	explicit DalSoRiR2(const DeviceConfig& config);
	~DalSoRiR2() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte readMem(word addr, EmuTime::param time) override;
	byte peekMem(word addr, EmuTime::param time) const override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) override;
	void writeMem(word addr, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRegCfg(byte value);
	byte* getSramAddr(word addr);
	unsigned getFlashAddr(word addr) const;

	void setupMemPtrs(
		bool mode0,
		std::span<const uint8_t> rom,
		std::span<const uint8_t> ram,
		std::span<YMF278::Block128, 32> memPtrs);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	YMF278B ymf278b;

	Ram sram;
	AmdFlash flash;

	BooleanSetting dipSwitchBDIS;
	BooleanSetting dipSwitchMCFG;
	BooleanSetting dipSwitchIO_C0;
	BooleanSetting dipSwitchIO_C4;

	std::array<byte, 4> regBank = {};
	std::array<byte, 2> regFrame = {};
	byte regCfg = 0;

	bool biosDisable;
};

} // namespace openmsx

#endif
