// $Id$

#ifndef __AUDIOINPUTCONNECTOR_HH__
#define __AUDIOINPUTCONNECTOR_HH__

#include "Connector.hh"

class DummyAudioInputDevice;
class WavAudioInput;


class AudioInputConnector : public Connector
{
	public:
		AudioInputConnector();
		virtual ~AudioInputConnector();
	
		// Connector
		virtual const string &getClass() const;
		virtual void plug(Pluggable *dev, const EmuTime &time);
		virtual void unplug(const EmuTime &time);
		
		short readSample(const EmuTime &time);

	private:
		DummyAudioInputDevice* dummy;
		WavAudioInput* wavInput;
};

#endif
