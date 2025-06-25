#ifndef MSXMATSUSHITA_HH
#define MSXMATSUSHITA_HH

#include "EmuTime.hh"
#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "FirmwareSwitch.hh"
#include "Clock.hh"
#include "serialize_meta.hh"

#include <cstdint>

namespace openmsx {

using uint4_t = uint8_t;

class MSXCPU;
class SRAM;
class VDP;

class MSXMatsushita final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXMatsushita(const DeviceConfig& config);
	void init() override;
	~MSXMatsushita() override;

	// MSXDevice
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	// MSXSwitchedDevice
	[[nodiscard]] uint8_t readSwitchedIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekSwitchedIO(uint16_t port, EmuTime time) const override;
	void writeSwitchedIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void unwrap();
	void delay(EmuTime time);

private:
	MSXCPU& cpu;
	VDP* vdp = nullptr;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<5369318> lastTime{EmuTime::zero()}; // 5.3MHz = 3.5MHz * 3/2

	FirmwareSwitch firmwareSwitch;
	const std::unique_ptr<SRAM> sram; // can be nullptr
	uint16_t address;
	uint4_t color1, color2;
	uint8_t pattern;
	const bool turboAvailable;
	bool turboEnabled = false;
};
SERIALIZE_CLASS_VERSION(MSXMatsushita, 2);

} // namespace openmsx

#endif
