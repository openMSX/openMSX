// $Id$

#include "MSXMoonSound.hh"
#include "YMF262.hh"
#include "YMF278.hh"
#include "XMLElement.hh"
#include "Mixer.hh"

namespace openmsx {

MSXMoonSound::MSXMoonSound(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	int ramSize = config.getChildDataAsInt("sampleram", 512); // size in kb
	ymf262.reset(new YMF262(getName() + " FM", config, time));
	ymf278.reset(new YMF278(getName() + " wave", ramSize, config, time));
	reset(time);
}

MSXMoonSound::~MSXMoonSound()
{
}

void MSXMoonSound::reset(const EmuTime& time)
{
	ymf262->reset(time);
	ymf278->reset(time);

	// TODO check
	opl4latch = 0;
	opl3latch = 0;
}

byte MSXMoonSound::readIO(byte port, const EmuTime& time)
{
	Mixer::instance().updateStream(time); // TODO optimize
	byte result;
	if (port < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			result = 255;
			break;
		case 1: // read wave register
			result = ymf278->readReg(opl4latch, time);
			break;
		default: // unreachable, avoid warning
			assert(false);
			result = 255;
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
			assert(false);
			result = 255;
		}
	}
	//PRT_DEBUG("MoonSound: read "<<hex<<(int)port<<" "<<(int)result<<dec);
	return result;
}

byte MSXMoonSound::peekIO(byte port, const EmuTime& time) const
{
	byte result;
	if (port < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			result = 255;
			break;
		case 1: // read wave register
			result = ymf278->peekReg(opl4latch);
			break;
		default: // unreachable, avoid warning
			assert(false);
			result = 255;
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
			assert(false);
			result = 255;
		}
	}
	return result;
}

void MSXMoonSound::writeIO(byte port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("MoonSound: write "<<hex<<(int)port<<" "<<(int)value<<dec);
	if (port < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // select register
			opl4latch = value;
			break;
		case 1:
			ymf278->writeRegOPL4(opl4latch, value, time);
			break;
		default:
			assert(false);
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
			assert(false);
		}
	}
}

} // namespace openmsx
