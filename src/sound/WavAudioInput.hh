// $Id$

#ifndef __WAVAUDIOINPUT_HH__
#define __WAVAUDIOINPUT_HH__

#include <SDL/SDL.h>
#include "AudioInputDevice.hh"
#include "EmuTime.hh"


class WavAudioInput : public AudioInputDevice
{
	public:
		WavAudioInput();
		virtual ~WavAudioInput();
		
		// AudioInputDevice
		virtual const string& getName() const;
		virtual void plug(Connector* connector, const EmuTime &time);
		virtual void unplug(const EmuTime &time);
		virtual short readSample(const EmuTime &time);

	private:
		int length;
		Uint8* buffer;
		int freq;
		EmuTime reference;
};

#endif
