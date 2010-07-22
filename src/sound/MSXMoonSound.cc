// $Id$

#include "MSXMoonSound.hh"
#include "YMF262.hh"
#include "YMF278.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

// Verified on a real YMF278:
//  When bit NEW2=0, writes to the wave-part ports (both register select and
//  register data) are ignored. But reads happen normally. Also reads from the
//  sample memory happen normally (and thus increase the internal memory
//  pointer).

MSXMoonSound::MSXMoonSound(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, ymf262(new YMF262(motherBoard, getName() + " FM", config, true))
	, ymf278(new YMF278(motherBoard, getName() + " wave",
	                    config.getChildDataAsInt("sampleram", 512), // size in kb
	                    config))
{
	powerUp(getCurrentTime());
}

MSXMoonSound::~MSXMoonSound()
{
}

void MSXMoonSound::powerUp(EmuTime::param time)
{
	ymf278->clearRam();
	reset(time);
}

void MSXMoonSound::reset(EmuTime::param time)
{
	ymf262->reset(time);
	ymf278->reset(time);

	// TODO check
	opl4latch = 0;
	opl3latch = 0;
}

byte MSXMoonSound::readIO(word port, EmuTime::param time)
{
	byte result;
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			result = 255;
			break;
		case 1: // read wave register
			result = ymf278->readReg(opl4latch, time);
			break;
		default: // unreachable, avoid warning
			UNREACHABLE; result = 255;
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			result = ymf262->readStatus() |
			         ymf278->readStatus(time);
			break;
		case 1:
		case 3: // read fm register
			result = ymf262->readReg(opl3latch);
			break;
		default: // unreachable, avoid warning
			UNREACHABLE; result = 255;
		}
	}
	return result;
}

byte MSXMoonSound::peekIO(word port, EmuTime::param time) const
{
	byte result;
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			result = 255;
			break;
		case 1: // read wave register
			result = ymf278->peekReg(opl4latch);
			break;
		default: // unreachable, avoid warning
			UNREACHABLE; result = 255;
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			result = ymf262->peekStatus() |
			         ymf278->peekStatus(time);
			break;
		case 1:
		case 3: // read fm register
			result = ymf262->peekReg(opl3latch);
			break;
		default: // unreachable, avoid warning
			UNREACHABLE; result = 255;
		}
	}
	return result;
}

void MSXMoonSound::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port&0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		if (ymf262->peekReg(0x105) & 0x02) { // NEW2 bit
			switch (port & 0x01) {
			case 0: // select register
				opl4latch = value;
				break;
			case 1:
				ymf278->writeRegOPL4(opl4latch, value, time);
				break;
			default:
				UNREACHABLE;
			}
		} else {
			// writes are ignore when NEW2=0
		}
	} else {
		// FM part  0xC4-0xC7
		switch (port & 0x03) {
		case 0: // select register bank 0
			opl3latch = value;
			break;
		case 2: // select register bank 1
			opl3latch = value | 0x100;
			break;
		case 1:
		case 3: // write fm register
			ymf262->writeReg(opl3latch, value, time);
			break;
		default:
			UNREACHABLE;
		}
	}
}


template<typename Archive>
void MSXMoonSound::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ymf262", *ymf262);
	ar.serialize("ymf278", *ymf278);
	ar.serialize("opl3latch", opl3latch);
	ar.serialize("opl4latch", opl4latch);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMoonSound);
REGISTER_MSXDEVICE(MSXMoonSound, "MoonSound");

} // namespace openmsx
