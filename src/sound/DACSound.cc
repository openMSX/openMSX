// $Id$

/**
 * Emulation of an 8bit unsigned DAC audio device
 */

#include "DACSound.hh"
#include "Mixer.hh"


DACSound::DACSound(short maxVolume, int typicalFreq, const EmuTime &time)
{
	PRT_DEBUG("Creating DACSound device");
	mixer = Mixer::instance();
	
	setVolume(maxVolume);
	lastValue = CENTER;
	reset(time);
	
	int bufSize = mixer->registerSound(this);
	buf = new int[bufSize];
}

DACSound::~DACSound()
{
	PRT_DEBUG("Destroying DACSound device");
	
	mixer->unregisterSound(this);
	delete[] buf;
}

void DACSound::setInternalVolume(short newVolume)
{
	// calculate the volume->voltage conversion table
	// The DAC uses 8 bit unsigned wave data
	// 0x00 is max negative, 0xFF max positive
	// 0x80 is centre (no amplitude)

	double scale = (double)(newVolume / CENTER);	// scale factor for DAC
	for (int i=0; i<256; i++) {
		volTable[i] = (short)(scale * (i-CENTER));
	}
}

void DACSound::setSampleRate (int smplRt)
{
	// ignore sampleRate
}

void DACSound::reset(const EmuTime &time)
{
	writeDAC(CENTER, time);
}

void DACSound::writeDAC(byte value, const EmuTime &time)
{
	mixer->updateStream(time);
	
	lastValue = value;
	setInternalMute(value == CENTER);
}

int* DACSound::updateBuffer(int length)
{
	if (isInternalMuted())
		return NULL;
	
	int* buffer = buf;
	while (length--)
		*buffer++ = volTable[lastValue];
	return buf;
}
