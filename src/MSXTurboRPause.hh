/*
 * This class implements the 2 Turbo-R specific LEDS:
 *
 * Bit 0 of IO-port 0xA7 turns the PAUSE led ON (1) or OFF (0)
 * Bit 7                           TURBO
 * TODO merge doc below
 */
#ifndef TURBORPAUSE_HH
#define TURBORPAUSE_HH

#include "MSXDevice.hh"
#include "BooleanSetting.hh"
#include "Observer.hh"

namespace openmsx {

/**
 * This class implements the MSX Turbo-R pause key
 *
 *  Whenever the pause key is pressed a flip-flop is toggled.
 *  The status of this flip-flop can be read from io-port 0xA7.
 *   bit 0 indicates the status (1 = pause active)
 *   all other bits read 0
 */
class MSXTurboRPause final : public MSXDevice, private Observer<Setting>
{
public:
	explicit MSXTurboRPause(const DeviceConfig& config);
	~MSXTurboRPause() override;

	void reset(EmuTime::param time) override;
	void powerDown(EmuTime::param time) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void updatePause();

private:
	BooleanSetting pauseSetting;
	byte status = 255;
	bool pauseLed = false;
	bool turboLed = false;
	bool hwPause = false;
};

} // namespace openmsx

#endif
