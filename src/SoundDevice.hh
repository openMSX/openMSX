// $Id$

#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include "openmsx.hh"

class SoundDevice
{
	public:
		/**
		 * Set the relative volume for this sound device, this
		 * can be used to make a MSX-MUSIC sound louder than a
		 * MSX-AUDIO  (0 <= newVolume <= 32767)
		 *
		 * The SoundDevice itself should call this method once at
		 * initialization (preferably with a value from the config
		 * file). Later on the user might (interactively) alter the
		 * volume of this device
		 */
		virtual void setVolume (short newVolume) = 0;

		/**
		 * When a SoundDevice registers itself with the Mixer, the
		 * Mixer sets the required sampleRate through this method. All
		 * sound devices share a common sampleRate.
		 * It is not possible to change the samplerate on-the-fly.
		 */
		virtual void setSampleRate (int newSampleRate) = 0;

		/**
		 * This method is regularly called from the Mixer, it should
		 * return a pointer to a buffer filled with the required number
		 * of samples. Samples are always 16-bit signed
		 *
		 * Note: this method runs in a different thread, you can
		 *       (un)lock this thread with SDL_LockAudio() and
		 *       SDL_UnlockAudio()
		 */
		virtual int* updateBuffer (int length) = 0;
};
#endif
