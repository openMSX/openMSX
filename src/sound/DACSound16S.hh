// $Id$

// This class implements a 16 bit signed DAC

#ifndef __DACSOUND16S_HH__
#define __DACSOUND16S_HH__

#include <string>
#include "openmsx.hh"
#include "SoundDevice.hh"

using std::string;

namespace openmsx {

class DACSound16S : public SoundDevice
{
public:
	DACSound16S(const string& name, const string& desc,
		    const XMLElement& config, const EmuTime& time); 
	virtual ~DACSound16S();

	void reset(const EmuTime& time);
	void writeDAC(short value, const EmuTime& time);
	
	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(int length, int* buffer);
	
private:
	short lastWrittenValue;
	int sample;
	int volume;

	const string name;
	const string desc;
};

} // namespace openmsx

#endif
