#ifndef PIONEERLDCONTROL_HH
#define PIONEERLDCONTROL_HH

#include "LaserdiscPlayer.hh"
#include "MSXDevice.hh"
#include "Clock.hh"
#include "IRQHelper.hh"
#include "Rom.hh"
#include <optional>

namespace openmsx {

class MSXPPI;
class VDP;

class PioneerLDControl final : public MSXDevice
{
public:
	explicit PioneerLDControl(const DeviceConfig& config);
	~PioneerLDControl() override;

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;
	void init() override;

	void videoIn(bool enabled);

	void serialize(Archive auto& ar, unsigned version);

private:
	void updateVideoSource();

private:
	Rom rom;
	std::optional<LaserdiscPlayer> laserdisc; // can be nullopt
	MSXPPI* ppi;
	VDP* vdp;

	/**
	 * The reference clock for the pulse is 500kHz / 128, which
	 * alternates after each period. We double the frequency
	 * and use modulo 2 for whether it will be high or low.
	 *
	 * See page 88 of the PX-7 Service Manual.
	 */
	Clock<2 * 500000, 128> clock;	// 2*500kHz / 128 = 7812.5Hz

	/**
	 * When video output stops, generate an IRQ
	 */
	IRQHelper irq;

	bool extint;
	bool muteL, muteR;
	bool videoEnabled;
	bool superimposing;
};

} // namespace openmsx

#endif
