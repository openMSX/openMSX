// This class implements the an 8 bit unsigned DAC to produce sound
// This are used in Konami's Synthesizer, Majutsushi (Mah Jong 2)RC 765
// And for SIMPL 

#ifndef __DACSOUND_HH__
#define __DACSOUND_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "emutime.hh"


class DACSound : public SoundDevice
{
	public:
		DACSound(); 
		virtual ~DACSound(); 
	
		byte readDAC(byte value, const Emutime &time);
		void writeDAC(byte value, const Emutime &time);

		//SoundDevice
		void init();
		void reset();
		void setVolume(short newVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);
		
	private:
		static const int CLOCK = 3579545;	// real time clock frequency of DACSound
							// for the moment equals the Z80 frequence
		int clocksPerSample;
		Emutime lastChanged;
		int delta;
		unsigned short volTable[256];
		struct {
		  int time;
		  short sample;
		} audiobuffer[256]; // hint: a cyclicbuffer and the index counter is a byte
		byte bufreadindex;
		byte bufwriteindex;
		byte DACValue;
		short DACSample;

		int* buf;
};
#endif
