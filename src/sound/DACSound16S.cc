// $Id$

#include "DACSound16S.hh"
#include "Mixer.hh"


DACSound16S::DACSound16S(short maxVolume, const EmuTime &time)
{
	mixer = Mixer::instance();
	
	setVolume(maxVolume);
	lastValue = 0;
	reset(time);
	
	int bufSize = mixer->registerSound(this);
	buf = new int[bufSize];
}

DACSound16S::~DACSound16S()
{
	mixer->unregisterSound(this);
	delete[] buf;
}

void DACSound16S::setInternalVolume(short newVolume)
{
	volume = newVolume;
}

void DACSound16S::setSampleRate (int smplRt)
{
	// ignore sampleRate
}

void DACSound16S::reset(const EmuTime &time)
{
	writeDAC(0, time);
}

void DACSound16S::writeDAC(short value, const EmuTime &time)
{
	mixer->updateStream(time);
	
	lastValue = (volume * value) >> 15;
	setInternalMute(lastValue == 0);
}

int* DACSound16S::updateBuffer(int length)
{
	if (isInternalMuted()) {
		return NULL;
	}
	int* buffer = buf;
	while (length--) {
		*buffer++ = lastValue;
	}
	return buf;
}
