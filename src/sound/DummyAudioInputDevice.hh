// $Id$

#ifndef __DUMMYAUDIOINPUTDEVICE_HH__
#define __DUMMYAUDIOINPUTDEVICE_HH__

#include "AudioInputDevice.hh"

namespace openmsx {

class DummyAudioInputDevice : public AudioInputDevice
{
public:
	DummyAudioInputDevice();

	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	virtual short readSample(const EmuTime& time);
};

} // namespace openmsx

#endif
