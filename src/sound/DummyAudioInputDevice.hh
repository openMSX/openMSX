// $Id$

#ifndef DUMMYAUDIOINPUTDEVICE_HH
#define DUMMYAUDIOINPUTDEVICE_HH

#include "AudioInputDevice.hh"

namespace openmsx {

class DummyAudioInputDevice : public AudioInputDevice
{
public:
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual short readSample(const EmuTime& time);
};

} // namespace openmsx

#endif
