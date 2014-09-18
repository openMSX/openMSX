#ifndef DUMMYAUDIOINPUTDEVICE_HH
#define DUMMYAUDIOINPUTDEVICE_HH

#include "AudioInputDevice.hh"

namespace openmsx {

class DummyAudioInputDevice final : public AudioInputDevice
{
public:
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual short readSample(EmuTime::param time);
};

} // namespace openmsx

#endif
