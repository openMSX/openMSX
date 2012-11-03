// $Id$

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
#include "Observer.hh"
#include <memory>

namespace openmsx {

class BooleanSetting;
class Setting;

/**
 * This class implements the MSX Turbo-R pause key
 *
 *  Whenever the pause key is pressed a flip-flop is toggled.
 *  The status of this flip-flop can be read from io-port 0xA7.
 *   bit 0 indicates the status (1 = pause active)
 *   all other bits read 0
 */
class MSXTurboRPause : public MSXDevice, private Observer<Setting>
{
public:
	explicit MSXTurboRPause(const DeviceConfig& config);
	virtual ~MSXTurboRPause();

	virtual void reset(EmuTime::param time);
	virtual void powerDown(EmuTime::param time);

	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);


private:
	// Observer<Setting>
	void update(const Setting& setting);

	void updatePause();

	const std::unique_ptr<BooleanSetting> pauseSetting;
	byte status;
	bool pauseLed;
	bool turboLed;
	bool hwPause;
};

} // namespace openmsx

#endif
