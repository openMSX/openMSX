// $Id$

// This class implements a 16 bit signed DAC

#ifndef DACSOUND16S_HH
#define DACSOUND16S_HH

#include <string>
#include "openmsx.hh"
#include "SoundDevice.hh"

namespace openmsx {

class DACSound16S : public SoundDevice
{
public:
	DACSound16S(const std::string& name, const std::string& desc,
		    const XMLElement& config, const EmuTime& time); 
	virtual ~DACSound16S();

	void reset(const EmuTime& time);
	void writeDAC(short value, const EmuTime& time);
	
	// SoundDevice
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(int length, int* buffer);
	
private:
	short lastWrittenValue;
	int sample;
	int volume;

	const std::string name;
	const std::string desc;
};

} // namespace openmsx

#endif
