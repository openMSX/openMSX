// $Id$

#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include <string>

using std::string;

namespace openmsx {

class SoundDevice
{
	public:
		// Constructor
		SoundDevice();

		/**
		 * Get name
		 */
		virtual const string& getName() const = 0;

		/**
		 * Get description
		 */
		virtual const string& getDescription() const = 0;
		
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
		void setVolume(short newVolume);

		/**
		 * This method can be used to show the current volume settings
		 * of the devices in some sort of UI
		 */
		short getVolume() const;

		/**
		 * This method mutes this SoundDevice
		 *  false -> not muted
		 *  true  -> muted
		 * This method should only be used by the UI, internal muting
		 * must be done with setInternalMute()
		 */
		void setMute(bool muted);

		/**
		 * Get the mute-status
		 *  false -> not muted
		 *  true  -> muted
		 * This method should only be used by the UI, checking if the
		 * devices needs to be mixed must be done with isInternalMute()
		 */
		bool isMuted() const;

	protected:
		/**
		 * This method is called from within setVolume(). It is (except
		 * for initialization) only called from user interaction, so
		 * the volume change must not have an immediate effect, for
		 * example precalculated buffers must not be discarded.
		 */
		virtual void setInternalVolume(short newVolume) = 0;
		
		/**
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
		void setInternalMute(bool muted);

		/**
		 * Returns true when for some reason this device is muted.  
		 * (methods setMute() and setInternalMute())
		 */
		bool isInternalMuted() const;
		
	// may only be called by Mixer
	public:
		/**
		 * When a SoundDevice registers itself with the Mixer, the
		 * Mixer sets the required sampleRate through this method. All
		 * sound devices share a common sampleRate.
		 * It is not possible to change the samplerate on-the-fly.
		 */
		virtual void setSampleRate(int newSampleRate) = 0;

		/**
		 * This method is regularly called from the Mixer, it should
		 * return a pointer to a buffer filled with the required number
		 * of samples. Samples are always 16-bit signed.
		 * If a device is muted (method isInternalMuted()) this method
		 * should return a null-pointer.
		 *
		 * Note: this method runs in a different thread, you can
		 *       (un)lock this thread with lock() and unlock()
		 *       methods in Mixer
		 */
		virtual int* updateBuffer(int length) = 0;

	private:
		short volume;
		bool userMuted, internalMuted;
};

} // namespace openmsx
#endif
