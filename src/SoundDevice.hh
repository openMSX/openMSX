// $Id$

#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include "openmsx.hh"

class SoundDevice
{
	public:
		// Constructor
		SoundDevice();
	
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
		void setVolume (short newVolume);

		/**
		 * This method can be used to show the current volume settings
		 * of the devices in some sort of UI
		 */
		short getVolume();

		/**
		 * This method mutes this SoundDevice
		 *  false -> not muted
		 *  true  -> muted
		 * This method should only be used by the UI, internal muting
		 * must be done with setInternalMute()
		 */
		void setMute (bool muted);

		/**
		 * Get the mute-status
		 *  false -> not muted
		 *  true  -> muted
		 * This method should only be used by the UI, checking if the
		 * devices needs to be mixed must be done with isInternalMute()
		 */
		bool isMuted();


	protected:
		/*
		 * This method is called from within setVolume(). It is (except
		 * for initialization) only called from user interaction, so
		 * the volume change must not have an immediate effect, for
		 * example precalculated buffers must not be discarded.
		 */
		virtual void setInternalVolume (short newVolume) = 0;
		
		/*
		 * This method mutes this SoundDevice
		 *  false -> not muted
		 *  true  -> muted
		 * This method exists purely for optimizations. If the device
		 * doesn't produce sound for the moment it can be muted, then 
		 * the Mixer doesn't ask for buffer anymore
		 *
		 * Note: internalMute defaults to true after SoundDevice creation,
		 *       don't forget to unmute it
		 */
		void setInternalMute (bool muted);


	// may only be called by Mixer
	public:
		/*
		 * The mixer uses this method to check if this device is 
		 * producing sound, if not it will not ask to fill buffers
		 */
		bool isInternalMuted();
		
		/*
		 * When a SoundDevice registers itself with the Mixer, the
		 * Mixer sets the required sampleRate through this method. All
		 * sound devices share a common sampleRate.
		 * It is not possible to change the samplerate on-the-fly.
		 */
		virtual void setSampleRate (int newSampleRate) = 0;

		/*
		 * This method is regularly called from the Mixer, it should
		 * return a pointer to a buffer filled with the required number
		 * of samples. Samples are always 16-bit signed
		 *
		 * Note: this method runs in a different thread, you can
		 *       (un)lock this thread with SDL_LockAudio() and
		 *       SDL_UnlockAudio()
		 */
		virtual int* updateBuffer (int length) = 0;

	private:
		short volume;
		bool userMuted, internalMuted;
};
#endif
