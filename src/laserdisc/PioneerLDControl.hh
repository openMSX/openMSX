// $Id$

#ifndef PIONEERLDCONTROL_HH
#define PIONEERLDCONTROL_HH

#include "MSXDevice.hh"
#include "Clock.hh"
#include "IRQHelper.hh"
#include <memory>

namespace openmsx {

class Rom;
class LaserdiscPlayer;
class MSXPPI;
class VDP;

class PioneerLDControl : public MSXDevice
{
public:
	explicit PioneerLDControl(const DeviceConfig& config);
	virtual ~PioneerLDControl();

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word address) const;
	virtual void init();

	void videoIn(bool enabled);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void updateVideoSource();

	const std::auto_ptr<Rom> rom;
	std::auto_ptr<LaserdiscPlayer> laserdisc;
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
	bool mutel, muter;
	bool videoEnabled;
	bool superimposing;
};

} // namespace openmsx

#endif
