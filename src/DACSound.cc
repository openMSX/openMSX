/**
 *
 * Emulation of an 8bit unsigned DAC audio device
 *
 */


#include <assert.h>
#include "DACSound.hh"
#include "Mixer.hh"
#include "MSXRealTime.hh"


DACSound::DACSound(short maxVolume)
{
	PRT_DEBUG("DAC audio created");
	realtime = MSXRealTime::instance();
	
	int bufSize = Mixer::instance()->registerSound(this);
	buf = new int[bufSize];
	
	setVolume(maxVolume);
	reset();
}


DACSound::~DACSound()
{
	PRT_DEBUG("DAC audio destroyed");
	delete[] buf;
}

void DACSound::reset()
{
	bufWriteIndex = bufReadIndex = 0;	// empty sample buffer
	DACValue = CENTER;
	left = 1; tempVal = 0;
	setInternalMute(true);
}

byte DACSound::readDAC(byte value, const Emutime &time)
{
	return DACValue;
}


void DACSound::writeDAC(byte value, const Emutime &time)
{
	DACValue = value;
	
	// duration is number of samples since last writeDAC (this is a float!!)
	float duration = realtime->getRealDuration(ref, time) * sampleRate;
	ref = time;
	
	if (left != 1) {
		// complete a previously written sample 
		if (duration >= left) {
			// complete till end of sample
			insertSamples(1, tempVal + left*volTable[value]); // 1 interpolated sample
			duration -= left;
			left = 1;
			tempVal = 0;
		} else {
			// not enough to complete sample
			tempVal += duration*volTable[value];
			left -= duration;
			return;
		}
	}
	// at the beginning of a sample,
	assert((left == 1) && (tempVal == 0));
	int wholeSamples = (int)duration;
	if (wholeSamples > 0) {
		insertSamples(wholeSamples, volTable[value]); // some samples at a constant level
	}
	duration -= wholeSamples;
	if (duration > 0) {
		// set first part of sample 
		tempVal = duration*volTable[value];
		left -= duration;
	}

	PRT_DEBUG("DAC unmuted");
	setInternalMute(false);	// set not muted
}

void DACSound::insertSamples(int nbSamples, short sample)
{
	//TODO check for overflow
	audioBuffer[bufWriteIndex].nbSamples = nbSamples;
	audioBuffer[bufWriteIndex].sample    = sample;
	bufWriteIndex = (bufWriteIndex+1) % BUFSIZE;
	assert(bufWriteIndex!=bufReadIndex);
}

void DACSound::setInternalVolume(short newVolume)
{
	// calculate the volume->voltage conversion table
	// The DAC uses 8 bit unsigned wave data
	// 0x00 is max negative, 0xFF max positive
	// 0x80 is centre (no amplitude)

	double scale = (double)(newVolume / CENTER);	// scale factor for DAC
	PRT_DEBUG("DAC scale : " << scale );
	for (int i=0; i<256; i++) {
		volTable[i] = (short)(scale * (i-CENTER));
	}
}


void DACSound::setSampleRate (int smplRt)
{
	sampleRate = smplRt;
}


int* DACSound::updateBuffer(int length)
{
	int* buffer = buf;
	
	while (length > 0) {
		if (bufReadIndex == bufWriteIndex) {
			// no more data in buffer, output is last written sample
			for (;length>0; length--) {
				*(buffer++) = volTable[DACValue];
			}
			if (DACValue == CENTER)
				PRT_DEBUG("DAC muted");
				setInternalMute(true);	// set muted
		} else {
			// update from bufreadindex
			int nbSamples = audioBuffer[bufReadIndex].nbSamples;
			short sample = audioBuffer[bufReadIndex].sample;

			if (nbSamples <= length) {
				length -= nbSamples;
				for (;nbSamples>0; nbSamples--) {
					*(buffer++) = sample;
				}
				bufReadIndex = (bufReadIndex+1) % BUFSIZE;
			} else {
				audioBuffer[bufReadIndex].nbSamples -= length;
				for (;length>0; length--) {
					*(buffer++) = sample;
				}
			}
		}
	}
	return buf;
}
