
#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include "openmsx.hh"

class SoundDevice
{
	public:
		/**
		 * Set the relative volume for this sound device, this
		 * can be used to make a MSX-MUSIC sound louder than a
		 * MSX-AUDIO
		 * 
		 *    0 <= newVolume <= 65535
		 */
		void setVolume (int newVolume);

		/**
		 * TODO update sound buffers
		 */
};
#endif
