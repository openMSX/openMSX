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

	explicit ChakkariCopy(const DeviceConfig& config);
	~ChakkariCopy();

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Observer<Setting>
	void update(const Setting& setting) override;

	Ram biosRam;
	Ram workRam;
	Rom rom;

	BooleanSetting pauseButtonPressedSetting;
	BooleanSetting copyButtonPressedSetting;
	EnumSetting<Mode> modeSetting;

	byte reg;
};

} // namespace openmsx

#endif
