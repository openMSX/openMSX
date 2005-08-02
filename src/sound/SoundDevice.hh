// $Id$

#ifndef SOUNDDEVICE_HH
#define SOUNDDEVICE_HH

#include "Mixer.hh"
#include <string>

namespace openmsx {

class XMLElement;

class SoundDevice
{
public:
	/**
	 * Get the unique name that identifies this sound device.
	 * Used to create setting names.
	 */
	const std::string& getName() const;

	/**
	 * Gets a description of this sound device,
	 * to be presented to the user.
	 */
	const std::string& getDescription() const;

	/**
	 * Set the relative volume for this sound device.
	 * This can be used for example to make an MSX-MUSIC sound louder
	 * than an MSX-AUDIO.
	 * Will be called by Mixer when the sound device registers.
	 * Later on the user might (interactively) alter the volume of this device.
	 * So the volume change must not necessarily have an immediate effect,
	 * for example (short) precalculated buffers must not be discarded.
	 * @param newVolume The new volume, where 0 = silent and 32767 = max.
	 */
	virtual void setVolume(int newVolume) = 0;

protected:
	/**
	 * Constructor.
	 * Initially, a sound device is muted.
	 * @param mixer The Mixer object
	 * @param name Unique name per sound device
	 * @param description Description for this sound device
	 */
	SoundDevice(Mixer& mixer, const std::string& name,
	            const std::string& description);
	virtual ~SoundDevice();

	/**
	 * Mute/unmute this sound device.
	 * This method exists purely for optimizations. If the device doesn't
	 * produce sound at the moment it can be muted.
	 * @param muted True iff sound device is silent.
	 */
	void setMute(bool muted);

	/**
	 * Registers this sound device with the Mixer.
	 * Call this method when the sound device is ready to start receiving
	 * calls to updateBuffer, so after all initialisation is done.
	 * @param config Configuration data for this sound device.
	 * @param mode Mixer::MONO for a mono device, Mixer::STEREO for stereo.
	 */
	void registerSound(const XMLElement& config,
	                   Mixer::ChannelMode mode = Mixer::MONO);

	/**
	 * Unregisters this sound device with the Mixer.
	 * Call this method before any deallocation is done.
	 */
	void unregisterSound();

	/** Returns the Mixer associated with this SoundDevice
	 */
	Mixer& getMixer() const { return mixer; }

public: // Will be called by Mixer:
	/**
	 * When a SoundDevice registers itself with the Mixer, the Mixer sets
	 * the required sampleRate through this method. All sound devices share
	 * a common sampleRate.
	 * It is not possible to change the samplerate on-the-fly.
	 */
	virtual void setSampleRate(int newSampleRate) = 0;

	/**
	 * Is this sound device muted?
	 * @return true iff muted.
	 *
	 * Note: this method runs in a different thread, you can
	 *       (un)lock this thread with lock() and unlock()
	 *       methods in Mixer
	 */
	bool isMuted() const;

	/**
	 * Generate sample data
	 * @param length The number of required samples
	 * @param buffer This buffer should be filled
	 * @param start EmuTime of the first sample. This might not be 100%
	 *              exact, but it is guaranteed to increase monotonically.
	 * @param sampDur Estimated duration of one sample. Note that the
	 *                following equation is not always true (because it's
	 *                just an estimation, otherwise it is true)
	 *                  start + samples * length  !=  next(start)
	 *
	 * This method is regularly called from the Mixer, it should return a
	 * pointer to a buffer filled with the required number of samples.
	 * Samples are always ints, later they are converted to the systems
	 * native format (e.g. 16-bit signed).
	 * The Mixer will not call this method if the sound device is muted.
	 *
	 * Note: this method possibly runs in a different thread, you can
	 *       (un)lock this thread with the (un)lock() methods in Mixer
	 */
	virtual void updateBuffer(unsigned length, int* buffer,
	        const EmuTime& start, const EmuDuration& sampDur) = 0;

private:
	Mixer& mixer;
	const std::string name;
	const std::string description;

	bool muted;
};

} // namespace openmsx

#endif
