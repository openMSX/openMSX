// $Id$

// This class implements the an 8 bit unsigned DAC to produce sound
// This are used in Konami's Synthesizer, Majutsushi (Mah Jong 2)RC 765
// And for SIMPL 

#ifndef __DACSOUND_HH__
#define __DACSOUND_HH__

#include "SoundDevice.hh"
#include "EmuTime.hh"

// forward declarations
class Mixer;


class DACSound : public SoundDevice
{
	public:
		DACSound(short maxVolume, int typicalFreq, const EmuTime &time); 
		virtual ~DACSound();
	
		void reset(const EmuTime &time);
		void writeDAC(byte value, const EmuTime &time);
		
		//SoundDevice
		void setInternalVolume(short newVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);
		
	private:
		static const int CENTER = 0x80;
	
		byte lastValue;
		short volTable[256];
		int* buf;
		Mixer* mixer;
};
#endif
