// $Id$

// This class implements the an 8 bit unsigned DAC to produce sound
// This are used in Konami's Synthesizer, Majutsushi (Mah Jong 2)RC 765
// And for SIMPL 

#ifndef __DACSOUND_HH__
#define __DACSOUND_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"

// forward declarations
class Mixer;
class EmuTime;


class DACSound : public SoundDevice
{
	public:
		DACSound(short maxVolume, const EmuTime &time); 
		virtual ~DACSound();
	
		void reset(const EmuTime &time);
		void writeDAC(byte value, const EmuTime &time);
		
		//SoundDevice
		virtual void setInternalVolume(short newVolume);
		virtual void setSampleRate(int sampleRate);
		virtual int* updateBuffer(int length);
		
	private:
		static const int CENTER = 0x80;
	
		byte lastValue;
		short volTable[256];
		int* buf;
		Mixer* mixer;
};
#endif
