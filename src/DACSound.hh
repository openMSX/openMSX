// $Id$

// This class implements the an 8 bit unsigned DAC to produce sound
// This are used in Konami's Synthesizer, Majutsushi (Mah Jong 2)RC 765
// And for SIMPL 

#ifndef __DACSOUND_HH__
#define __DACSOUND_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include "MSXRealTime.hh"


class DACSound : public SoundDevice
{
	public:
		DACSound(short maxVolume, const EmuTime &time); 
		virtual ~DACSound(); 
	
		void reset(const EmuTime &time);
		byte readDAC(byte value, const EmuTime &time);
		void writeDAC(byte value, const EmuTime &time);
		
		//SoundDevice
		void setInternalVolume(short newVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);
		
	private:
		void insertSamples(int nbSamples, short sample);
		
		static const int BUFSIZE = 1024;
		static const int CENTER = 0x80;
	
		EmuTime ref;
		float left, tempVal;
		int sampleRate;

		short volTable[256];
		
		struct {
		  int nbSamples;
		  short sample;
		} audioBuffer[BUFSIZE];
		int bufReadIndex;
		int bufWriteIndex;
		
		byte DACValue;

		int* buf;
};
#endif
