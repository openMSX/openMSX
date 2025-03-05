#include "PioneerLDControl.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "MSXPPI.hh"
#include "MSXException.hh"
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
PioneerLDControl::PioneerLDControl(DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, irq(getMotherBoard(), "PioneerLDControl.IRQdisplayoff")
{
	if (config.getChildDataAsBool("laserdisc", true)) {
		laserdisc.emplace(getHardwareConfig(), *this);
	}
	reset(getCurrentTime());
}

void PioneerLDControl::init()
{
	MSXDevice::init();

	const auto& refs = getReferences();
	ppi = !refs.empty() ? dynamic_cast<MSXPPI*>(refs[0]) : nullptr;
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

PioneerLDControl::~PioneerLDControl() = default;

void PioneerLDControl::reset(EmuTime::param time)
{
	muteL = muteR = true;
	superimposing = false;
	extInt = false;

	irq.reset();
	if (laserdisc) laserdisc->setMuting(muteL, muteR, time);
}

byte PioneerLDControl::readMem(word address, EmuTime::param time)
{
	byte val = PioneerLDControl::peekMem(address, time);
	if (address == 0x7fff) {
		extInt = false;
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
		if (!extInt) {
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

const byte* PioneerLDControl::getReadCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return nullptr;
	} else if (0x4000 <= address && address < 0x6000) {
		return &rom[address & 0x1fff];
	} else {
		return unmappedRead.data();
	}
}

void PioneerLDControl::writeMem(word address, byte value, EmuTime::param time)
{
	if (address == 0x7fff) {
		// superimpose
		superimposing = !(value & 1);
		irq.set(superimposing && extInt);

		updateVideoSource();

		// Muting
		if (!muteL && !(value & 0x80)) {
			muteR = !(ppi->peekIO(2, time) & 0x10);
		}
		muteL = !(value & 0x80);
		if (laserdisc) laserdisc->setMuting(muteL, muteR, time);

	} else if (address == 0x7ffe) {
		if (laserdisc) laserdisc->extControl(value & 1, time);
	}
}

byte* PioneerLDControl::getWriteCacheLine(word address)
{
	if ((address & CacheLine::HIGH) == (0x7FFE & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

void PioneerLDControl::videoIn(bool enabled)
{
	if (videoEnabled && !enabled) {
		// raise an interrupt when external video goes off
		extInt = true;
		if (superimposing) irq.set();
	}
	videoEnabled = enabled;
	updateVideoSource();
}

void PioneerLDControl::updateVideoSource()
{
	const auto* videoSource = (videoEnabled && superimposing && laserdisc)
	                        ? laserdisc->getRawFrame()
	                        : nullptr;
	vdp->setExternalVideoSource(videoSource);
}

template<typename Archive>
void PioneerLDControl::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("clock", clock,
	             "mutel", muteL,
	             "muter", muteR);
	// videoEnabled is restored from LaserdiscPlayer. Set to false
	// for now so that the irq does not get changed during load
	if constexpr (Archive::IS_LOADER) {
		videoEnabled = false;
	}
	ar.serialize("superimposing", superimposing,
	             "extint",        extInt,
	             "irq",           irq);
	if (laserdisc) ar.serialize("laserdisc", *laserdisc);

	if constexpr (Archive::IS_LOADER) {
		updateVideoSource();
		if (laserdisc) {
			laserdisc->setMuting(muteL, muteR, getCurrentTime());
		}
	}
}

REGISTER_MSXDEVICE(PioneerLDControl, "PBASIC");
INSTANTIATE_SERIALIZE_METHODS(PioneerLDControl);

} // namespace openmsx
