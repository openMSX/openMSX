#ifndef CHAKKARICOPY_HH
#define CHAKKARICOPY_HH

#include "MSXDevice.hh"
#include "EnumSetting.hh"
#include "BooleanSetting.hh"
#include "Ram.hh"
#include "Rom.hh"

namespace openmsx {

class ChakkariCopy final : public MSXDevice, private Observer<Setting>
{
public:
	enum Mode { COPY, RAM };

	explicit ChakkariCopy(DeviceConfig& config);
	~ChakkariCopy() override;

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	Ram biosRam;
	Ram workRam;
	Rom rom;

	BooleanSetting pauseButtonPressedSetting;
	BooleanSetting copyButtonPressedSetting;
	EnumSetting<Mode> modeSetting;

	byte reg = 0xFF; // avoid UMR in initial writeIO()
};

} // namespace openmsx

#endif
