#ifndef MSXMATSUSHITA_HH
#define MSXMATSUSHITA_HH

#include "MSXDevice.hh"
#include "MSXSwitchedDevice.hh"
#include "FirmwareSwitch.hh"
#include "SRAM.hh"
#include "Clock.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXCPU;
class VDP;

class MSXMatsushita final : public MSXDevice, public MSXSwitchedDevice
{
public:
	explicit MSXMatsushita(const DeviceConfig& config);
	void init() override;
	~MSXMatsushita();

	// MSXDevice
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// MSXSwitchedDevice
	byte readSwitchedIO(word port, EmuTime::param time) override;
	byte peekSwitchedIO(word port, EmuTime::param time) const override;
	void writeSwitchedIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void delay(EmuTime::param time);

	MSXCPU& cpu;
	VDP* vdp;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<5369318> lastTime; // 5.3MHz = 3.5MHz * 3/2

	FirmwareSwitch firmwareSwitch;
	const std::unique_ptr<SRAM> sram; // can be nullptr
	word address;
	nibble color1, color2;
	byte pattern;
	const bool turboAvailable;
	bool turboEnabled;
};
SERIALIZE_CLASS_VERSION(MSXMatsushita, 2);

} // namespace openmsx

#endif
