// $Id$

#include "DACSound16S.hh"
#include "Mixer.hh"
#include "MSXCPU.hh"

namespace openmsx {

DACSound16S::DACSound16S(const string& name_, const string& desc_,
                         const XMLElement& config, const EmuTime& /*time*/)
	: name(name_), desc(desc_)
{
	lastWrittenValue = sample = 0;
	registerSound(config);
}

DACSound16S::~DACSound16S()
{
	unregisterSound();
}

const string& DACSound16S::getName() const
{
	return name;
}

const string& DACSound16S::getDescription() const
{
	return desc;
}

void DACSound16S::setVolume(int newVolume)
{
	volume = newVolume;
}

void DACSound16S::setSampleRate(int sampleRate)
{
	// nothing
}

void DACSound16S::reset(const EmuTime& time)
{
	writeDAC(0, time);
}

void DACSound16S::writeDAC(short value, const EmuTime& time)
{
	if (value == lastWrittenValue) {
		return;
	}
	Mixer::instance().updateStream(time);
	lastWrittenValue = value;
	sample = (value * volume) >> 15;
	setMute(value == 0);
}

int* DACSound16S::updateBuffer(int length)
{
	int* buf = buffer;
	while (length--) {
		*(buf++) = sample;
	}
	return buffer;
}

} // namespace openmsx

