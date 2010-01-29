// $Id$

#include "PioneerLDControl.hh"
#include "Rom.hh"
#include "XMLElement.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "LaserdiscPlayer.hh"
#include "MSXPPI.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "GlobalSettings.hh"
#include "Reactor.hh"
#include "VDP.hh"

namespace openmsx {

/*
 * Laserdisc Control: there are three bits involved here. There are three
 * bits/connections involved. 
 * - EXTACK (from Laserdisc -> MSX) will remain low for a while to acknowledge 
 *   it has received a command and is executing
 * - EXTCONTROL (from MSX -> Laserdisc) one bit information which is used for
 *   sending commands
 * - PULSE (internal to MSX) 8175.5hz signal which used by software to 
 *   create the right pulses for communicating with the Laserdisc over
 *   EXTCONTROL.
 *
 * Sound Muting: left and right audio channels from Laserdisc input can
 * be muted independently. After reset or startup both channels are muted. 
 * The left muting is controlled by bit 7 of 0x7fff; set is not muted,
 * cleared is muted. If this bit changed from 0->1 (rising edge triggered 
 * and inverted) then bit 4 of register C of PPI switches L muting; set
 * for muting disabled, clear for muting enabled.
 *
 * Cassette Input: If the motor relay is OFF then audio on the R channel
 * ends up in the PSG (regardless of muting); if the motor relay is ON
 * then normal tape input is used.
 */
PioneerLDControl::PioneerLDControl(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, laserdisc(new LaserdiscPlayer(motherBoard, *this, 
		motherBoard.getReactor().getGlobalSettings().getThrottleManager()))
	, clock(EmuTime::zero)
	, irq(motherBoard, "PioneerLDControl.IRQdisplayoff")
	, videoEnabled(false)
{
	reset(getCurrentTime());
}

void PioneerLDControl::init(const HardwareConfig& hwConf)
{
	MSXDevice::init(hwConf);

	const MSXDevice::Devices& references = getReferences();

	ppi = references.size() >= 1 ?
		dynamic_cast<MSXPPI*>(references[0]) : 0;
	if (!ppi) {
		throw MSXException("Invalid PioneerLDControl configuration: "
		                   "need reference to PPI device.");
	}

	vdp = references.size() == 2 ?
		dynamic_cast<VDP*>(references[1]) : 0;
	if (!vdp) {
		throw MSXException("Invalid PioneerLDControl configuration: "
		                   "need reference to VDP device.");
	}
}

PioneerLDControl::~PioneerLDControl()
{
}

void PioneerLDControl::reset(EmuTime::param time)
{
	mutel = muter = true;
	superimposing = false;

	irq.reset();
	laserdisc->setMuting(mutel, muter, time);
}

byte PioneerLDControl::readMem(word address, EmuTime::param time)
{
	byte val = PioneerLDControl::peekMem(address, time);
	if (address == 0x7fff) {
		if (irq.getState()) {
			irq.reset();
		}
	}
	return val;
}

byte PioneerLDControl::peekMem(word address, EmuTime::param time) const
{
	byte val = 0xff;

	if (address == 0x7fff) {
		if (videoEnabled) {
			val &= 0x7f;
		}
		if (!irq.getState()) {
			val &= 0xfe;
		}
	} else if (address == 0x7ffe) {
		if (clock.getTicksTill(time) & 1) {
			val &= 0xfe;
		}
		if (laserdisc->extAck(time)) {
			val &= 0x7f;
		}
	} else if (0x4000 <= address && address < 0x6000) {
		val = (*rom)[address & 0x1fff];
	}
	return val;
}

const byte* PioneerLDControl::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return NULL;
	} else if (0x4000 <= start && start < 0x6000) {
		return &(*rom)[start & 0x1fff];
	} else {
		return unmappedRead;
	}
}

void PioneerLDControl::writeMem(word address, byte value, EmuTime::param time)
{
	if (address == 0x7fff) {
		// superimpose
		superimposing = !(value & 1);
		updateVideoSource();

		// Muting
		if (!mutel && !(value & 0x80)) {
			muter = !(ppi->peekIO(2, time) & 0x10);
		}
		mutel = !(value & 0x80);
		laserdisc->setMuting(mutel, muter, time);

	} else if (address == 0x7ffe) {
		laserdisc->extControl(value & 1, time);
	}
}

byte* PioneerLDControl::getWriteCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

void PioneerLDControl::videoIn(bool enabled)
{
	if (superimposing && videoEnabled && !enabled) {
		irq.set();
	}
	videoEnabled = enabled;
	updateVideoSource();
}
void PioneerLDControl::updateVideoSource()
{
	const RawFrame* videoSource = (videoEnabled && superimposing)
	                            ? laserdisc->getRawFrame()
	                            : NULL;
	vdp->setExternalVideoSource(videoSource);
}

template<typename Archive>
void PioneerLDControl::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("clock", clock);
	ar.serialize("mutel", mutel);
	ar.serialize("muter", muter);
	// videoEnabled is restored from LaserdiscPlayer.
	ar.serialize("superimposing", superimposing);
	ar.serialize("irq", irq);
	ar.serialize("laserdisc", *laserdisc);

	if (ar.isLoader()) {
		updateVideoSource();
		laserdisc->setMuting(mutel, muter, getCurrentTime());
	}
}

REGISTER_MSXDEVICE(PioneerLDControl, "PBASIC");
INSTANTIATE_SERIALIZE_METHODS(PioneerLDControl);

} // namespace openmsx
