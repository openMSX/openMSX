// $Id$

// This class implements a 16 bit signed DAC

#ifndef __DACSOUND16S_HH__
#define __DACSOUND16S_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"

class Mixer;
class EmuTime;


class DACSound16S : public SoundDevice
{
	public:
		DACSound16S(short maxVolume, const EmuTime &time); 
		virtual ~DACSound16S();
	
		void reset(const EmuTime &time);
		void writeDAC(short value, const EmuTime &time);
		
		//SoundDevice
		virtual void setInternalVolume(short newVolume);
		virtual void setSampleRate(int sampleRate);
		virtual int* updateBuffer(int length);
		
	private:
		int volume;
		short lastValue;
		int* buf;
		Mixer* mixer;
};

#endif
