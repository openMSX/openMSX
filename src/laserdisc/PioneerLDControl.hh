// $Id$

#ifndef PIONEERLDCONTROL_HH
#define PIONEERLDCONTROL_HH

#include "Clock.hh"
#include "MSXDevice.hh"
#include "IRQHelper.hh"

namespace openmsx {

class MSXPPI;
class Rom;
class VDP;
class LaserdiscPlayer;

class PioneerLDControl : public MSXDevice
{
public:
	PioneerLDControl(MSXMotherBoard& motherBoard, const XMLElement& config);
	~PioneerLDControl();

	void reset(EmuTime::param time);
	byte readMem(word address, EmuTime::param time);
	byte peekMem(word address, EmuTime::param time);
	void writeMem(word address, byte value, EmuTime::param time);
	const byte* getReadCacheLine(word start) const;
	byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
	void videoIn(bool enabled);

private:
	void init(const HardwareConfig& hwConf);

	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<LaserdiscPlayer> laserdisc;
	MSXPPI *ppi;
	VDP *vdp;

	/**
	 * The reference clock for the pulse is 500kHz / 128, which
	 * alternates after each period. We double the frequency
	 * and use modulo 2 for whether it will be high or low.
	 *
	 * See page 88 of the PX-7 Service Manual.
	 */
	Clock<2 * 500000, 128> clock;	// 2*500kHz / 128 = 7812.5Hz

	bool mutel, muter;
	bool videoEnabled;
	bool superimposing;

	/**
	 * When video output stops, generate an IRQ
	 */
	IRQHelper irq;
};

} // namespace openmsx

#endif
