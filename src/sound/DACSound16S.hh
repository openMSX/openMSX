// $Id$

// This class implements a 16 bit signed DAC

#ifndef __DACSOUND16S_HH__
#define __DACSOUND16S_HH__

#include <list>
#include <string>
#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"

class DACSound16S : public SoundDevice
{
	public:
		DACSound16S(const string &name, short maxVolume,
		            const EmuTime &time); 
		virtual ~DACSound16S();
	
		void reset(const EmuTime &time);
		void writeDAC(short value, const EmuTime &time);
		
		//SoundDevice
		virtual void setInternalVolume(short newVolume);
		virtual void setSampleRate(int sampleRate);
		virtual int* updateBuffer(int length);
		
	private:
		inline int getSample(const EmuTime &time);
		
		struct Sample {
			Sample(int value_, const EmuTime &time_)
				: value(value_), time(time_) {}
			int value;
			EmuTime time;
		};
		list<Sample> samples;

		float oneSampDur;
		int lastValue;
		short lastWrittenValue;
		EmuTime lastTime;
		EmuTime nextTime;
		int volume;
		int* buffer;

		class MSXCPU* cpu;
		class RealTime* realTime;
};

#endif
