// $Id$

// This class implements a 16 bit signed DAC

#ifndef DACSOUND16S_HH
#define DACSOUND16S_HH

#include "SoundDevice.hh"
#include "BlipBuffer.hh"

namespace openmsx {

class DACSound16S : public SoundDevice
{
public:
	DACSound16S(MSXMixer& mixer, const std::string& name,
	            const std::string& desc, const XMLElement& config);
	virtual ~DACSound16S();

	void reset(EmuTime::param time);
	void writeDAC(short value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
	                          EmuTime::param time);

	BlipBuffer blip;
	short lastWrittenValue;
};

} // namespace openmsx

#endif
