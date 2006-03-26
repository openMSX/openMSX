// $Id$

// This class implements a 16 bit signed DAC

#ifndef DACSOUND16S_HH
#define DACSOUND16S_HH

#include "SoundDevice.hh"
#include "EmuTime.hh"
#include <deque>

namespace openmsx {

class DACSound16S : public SoundDevice
{
public:
	DACSound16S(Mixer& mixer, const std::string& name,
	            const std::string& desc, const XMLElement& config,
	            const EmuTime& time);
	virtual ~DACSound16S();

	void reset(const EmuTime& time);
	void writeDAC(short value, const EmuTime& time);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(unsigned length, int* buffer,
	        const EmuTime& start, const EmuDuration& sampDur);

private:
	struct Sample {
		Sample(const EmuTime& time_, int value_)
			: time(time_), value(value_) {}
		EmuTime time;
		int value;
	};
	typedef std::deque<Sample> Queue;
	Queue queue;

	int volume;
	int prevValue;
	int prevA, prevB;
	short lastWrittenValue;
};

} // namespace openmsx

#endif
