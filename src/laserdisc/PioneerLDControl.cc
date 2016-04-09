#include "PioneerLDControl.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "LaserdiscPlayer.hh"
#include "MSXPPI.hh"
#include "MSXException.hh"
#include "VDP.hh"
#include "memory.hh"

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
PioneerLDControl::PioneerLDControl(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, clock(EmuTime::zero)
	, irq(getMotherBoard(), "PioneerLDControl.IRQdisplayoff")
	, videoEnabled(false)
{
	if (config.getChildDataAsBool("laserdisc", true)) {
		laserdisc = make_unique<LaserdiscPlayer>(
			getHardwareConfig(), *this);
	}
	reset(getCurrentTime());
}

void PioneerLDControl::init()
{
	MSXDevice::init();

	const auto& refs = getReferences();
	ppi = refs.size() >= 1 ? dynamic_cast<MSXPPI*>(refs[0]) : nullptr;
	if (!ppi) {
		throw MSXException("Invalid PioneerLDControl configuration: "
		                   "need reference to PPI device.");
	}

	vdp = refs.size() == 2 ? dynamic_cast<VDP*>(refs[1]) : nullptr;
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
	extint = false;

	irq.reset();
	if (laserdisc) laserdisc->setMuting(mutel, muter, time);
}

byte PioneerLDControl::readMem(word address, EmuTime::param time)
{
	byte val = PioneerLDControl::peekMem(address, time);
	if (address == 0x7fff) {
		extint = false;
		irq.reset();
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
		if (!extint) {
			val &= 0xfe;
		}
	} else if (address == 0x7ffe) {
		if (clock.getTicksTill(time) & 1) {
			val &= 0xfe;
		}
		if (laserdisc && laserdisc->extAck(time)) {
			val &= 0x7f;
		}
	} else if (0x4000 <= address && address < 0x6000) {
		val = rom[address & 0x1fff];
	}
	return val;
}

const byte* PioneerLDControl::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return nullptr;
	} else if (0x4000 <= start && start < 0x6000) {
		return &rom[start & 0x1fff];
	} else {
		return unmappedRead;
	}
}

void PioneerLDControl::writeMem(word address, byte value, EmuTime::param time)
{
	if (address == 0x7fff) {
		// superimpose
		superimposing = !(value & 1);
		irq.set(superimposing && extint);

		updateVideoSource();

		// Muting
		if (!mutel && !(value & 0x80)) {
			muter = !(ppi->peekIO(2, time) & 0x10);
		}
		mutel = !(value & 0x80);
		if (laserdisc) laserdisc->setMuting(mutel, muter, time);

	} else if (address == 0x7ffe) {
		if (laserdisc) laserdisc->extControl(value & 1, time);
	}
}

byte* PioneerLDControl::getWriteCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

void PioneerLDControl::videoIn(bool enabled)
{
	if (videoEnabled && !enabled) {
		// raise an interrupt when external video goes off
		extint = true;
		if (superimposing) irq.set();
	}
	videoEnabled = enabled;
	updateVideoSource();
}

void PioneerLDControl::updateVideoSource()
{
	auto* videoSource = (videoEnabled && superimposing && laserdisc)
	                  ? laserdisc->getRawFrame()
	                  : nullptr;
	vdp->setExternalVideoSource(videoSource);
}

template<typename Archive>
void PioneerLDControl::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("clock", clock);
	ar.serialize("mutel", mutel);
	ar.serialize("muter", muter);
	// videoEnabled is restored from LaserdiscPlayer. Set to false
	// for now so that the irq does not get changed during load
	if (ar.isLoader()) {
		videoEnabled = false;
	}
	ar.serialize("superimposing", superimposing);
	ar.serialize("extint", extint);
	ar.serialize("irq", irq);
	if (laserdisc) ar.serialize("laserdisc", *laserdisc);

	if (ar.isLoader()) {
		updateVideoSource();
		if (laserdisc) {
			laserdisc->setMuting(mutel, muter, getCurrentTime());
		}
	}
}

REGISTER_MSXDEVICE(PioneerLDControl, "PBASIC");
INSTANTIATE_SERIALIZE_METHODS(PioneerLDControl);

} // namespace openmsx
