// $Id$

#ifndef __DUMMYAUDIOINPUTDEVICE_HH__
#define __DUMMYAUDIOINPUTDEVICE_HH__

#include "AudioInputDevice.hh"


namespace openmsx {

class DummyAudioInputDevice : public AudioInputDevice
{
	public:
		DummyAudioInputDevice();

		virtual void plug(Connector* connector, const EmuTime &time) throw();
		virtual void unplug(const EmuTime &time);

		virtual short readSample(const EmuTime &time);
};

} // namespace openmsx

#endif
