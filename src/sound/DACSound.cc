// $Id$

/**
 *
 * Emulation of an 8bit unsigned DAC audio device
 *
 */

#include <cassert>
#include "DACSound.hh"
#include "Mixer.hh"
#include "../cpu/MSXCPU.hh"


DACSound::DACSound(short maxVolume, int typicalFreq, const EmuTime &time) :
	emuDelay(1.0 / typicalFreq)
{
	PRT_DEBUG("Creating DACSound device");
	
	setVolume(maxVolume);
	currentValue = nextValue = tmpValue = volTable[CENTER];
	currentTime  = nextTime  = prevTime = lastTime = time;
	delta = left = currentLength = 0;
	lastValue = CENTER;
	reset(time);
	
	int bufSize = Mixer::instance()->registerSound(this);
	buf = new int[bufSize];
}


DACSound::~DACSound()
{
	PRT_DEBUG("Destroying DACSound device");
	Mixer::instance()->unregisterSound(this);
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

byte DACSound::readDAC(const EmuTime &time)
{
	return lastValue;
}

void DACSound::writeDAC(byte value, const EmuTime &time)
{
	// TODO temp DISABLED
	return;

	Mixer::instance()->lock();

	if (time > emuDelay) {
		EmuTime tmp = time - emuDelay;
		if (lastTime < tmp)
			insertSample(volTable[lastValue], tmp);
	}
	insertSample(volTable[value], time);
	lastTime = time;
	lastValue = value;
	
	if (value != CENTER)
		setInternalMute(false);
	
	Mixer::instance()->unlock();
}

void DACSound::insertSample(short sample, const EmuTime &time)
{
	Sample s = {sample, time};
	buffer.addBack(s);
}

int* DACSound::updateBuffer(int length)
{
	if (isInternalMuted())
		return NULL;
	
	int* buffer = buf;
	
	//EmuTime mixTime = MSXCPU::instance()->getCurrentTime() - emuDelay;
	EmuTime mixTime = MSXCPU::instance()->getCurrentTime(); // this is NOT accurate
	int timeUnit = mixTime.subtract(prevTime) / length;
	prevTime = mixTime;
	while (length) {
		if (left == 0) {
			// at the beginning of a sample
			if (currentLength >= timeUnit) {
				// enough for a whole sample
				*buffer++ = currentValue + delta/2;
				length--;
				currentLength -= timeUnit;
				currentValue += delta;
			} else {
				// not enough for a whole sample
				assert(timeUnit!=0);
				tmpValue = ((currentValue + delta/2) * currentLength) / timeUnit;
				left = timeUnit - currentLength;
				getNext(timeUnit);
			}
		} else {
			// not at the beginning of a sample
			if (currentLength >= left) {
				// enough to complete sample
				assert(timeUnit!=0);
				*buffer++ = tmpValue + ((currentValue + delta/2) * left) / timeUnit;
				length--;
				left = 0;
				currentLength -= left;
				currentValue += (delta*left)/timeUnit;
			} else {
				// not enough to complete sample
				assert(timeUnit!=0);
				tmpValue += ((currentValue + delta/2) * currentLength) / timeUnit;
				left -= currentLength;
				getNext(timeUnit);
			}
		}
	}
	return buf;
}

void DACSound::getNext(int timeUnit)
{
	// old next-values become the current-values
	currentValue = nextValue;
	currentTime = nextTime;
	if (!buffer.isEmpty()) {
		nextValue = buffer[0].sample;
		nextTime  = buffer[0].time;
		buffer.removeFront();
	} else {
		//nextValue = currentValue;
		nextTime = prevTime;	// = MixTime
		if (lastValue == CENTER)
			setInternalMute(true);
	}
	assert(nextTime >= currentTime);
	currentLength = nextTime.subtract(currentTime);
	if (currentLength != 0)
		delta = ((nextValue - currentValue) * timeUnit) / currentLength;
	//PRT_DEBUG("DAC: currentValue  " << currentValue);
	//PRT_DEBUG("DAC: currentTime   " << currentTime);
	//PRT_DEBUG("DAC: nextValue     " << nextValue);
	//PRT_DEBUG("DAC: nextTime      " << nextTime);
	//PRT_DEBUG("DAC: currentLength " << currentLength);
	//PRT_DEBUG("DAC: delta         " << delta);
}
