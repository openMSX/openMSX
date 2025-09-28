#ifndef CHAKKARICOPY_HH
#define CHAKKARICOPY_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "MSXDevice.hh"
#include "Ram.hh"
#include "Rom.hh"

#include <cstdint>

namespace openmsx {

class ChakkariCopy final : public MSXDevice, private Observer<Setting>
{
public:
	enum class Mode : uint8_t { COPY, RAM };

	explicit ChakkariCopy(DeviceConfig& config);
	~ChakkariCopy() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

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
