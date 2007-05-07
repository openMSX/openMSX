// $Id$

#ifndef SOUNDDEVICE_HH
#define SOUNDDEVICE_HH

#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMixer;
class XMLElement;
class EmuTime;
class EmuDuration;
class WavWriter;

class SoundDevice : private noncopyable
{
public:
	static const unsigned MAX_CHANNELS = 24;

	/** Get the unique name that identifies this sound device.
	  * Used to create setting names.
	  */
	const std::string& getName() const;

	/** Gets a description of this sound device,
	  * to be presented to the user.
	  */
	const std::string& getDescription() const;

	/** Is this a stereo device?
	  * This is set in the constructor and cannot be changed anymore
	  */
	bool isStereo() const;

	/**
	 * Set the relative volume for this sound device.
	 * This can be used for example to make an MSX-MUSIC sound louder
	 * than an MSX-AUDIO.
	 * Will be called by Mixer when the sound device registers.
	 * Later on the user might (interactively) alter the volume of this
	 * device. So the volume change must not necessarily have an immediate
	 * effect. For example (short) precalculated buffers must not be
	 * discarded.
	 * @param newVolume The new volume, where 0 = silent and 32767 = max.
	 */
	virtual void setVolume(int newVolume) = 0;

	void recordChannel(unsigned channel, const std::string& filename);
	void muteChannel  (unsigned channel, bool muted);

protected:
	/** Constructor.
	  * @param mixer The Mixer object
	  * @param name Unique name per sound device
	  * @param description Description for this sound device
	  * @param numChannels The number of channels for this device
	  * @param stereo Is this a stereo device
	  */
	SoundDevice(MSXMixer& mixer, const std::string& name,
	            const std::string& description,
	            unsigned numChannels, bool stereo = false);
	virtual ~SoundDevice();

	/**
	 * Registers this sound device with the Mixer.
	 * Call this method when the sound device is ready to start receiving
	 * calls to updateBuffer, so after all initialisation is done.
	 * @param config Configuration data for this sound device.
	 */
	void registerSound(const XMLElement& config);

	/**
	 * Unregisters this sound device with the Mixer.
	 * Call this method before any deallocation is done.
	 */
	void unregisterSound();

	/** @see Mixer::updateStream */
	void updateStream(const EmuTime& time);

	void setInputRate(unsigned sampleRate);

public: // Will be called by Mixer:
	/**
	 * When a SoundDevice registers itself with the Mixer, the Mixer sets
	 * the required sampleRate through this method. All sound devices share
	 * a common sampleRate.
	 */
	virtual void setOutputRate(unsigned sampleRate) = 0;

	/** Generate sample data
	  * @param length The number of required samples
	  * @param buffer This buffer should be filled
	  * @param start EmuTime of the first sample. This might not be 100%
	  *              exact, but it is guaranteed to increase monotonically.
	  * @param sampDur Estimated duration of one sample. Note that the
	  *                following equation is not always true (because it's
	  *                just an estimation, otherwise it is true)
	  *                  start + samples * length  !=  next(start)
	  * @result false iff the output is empty. IOW filling the buffer with
	  *         zeros or returning false has the same effect, but the latter
	  *         can be more efficient
	  *
	  * This method is regularly called from the Mixer, it should return a
	  * pointer to a buffer filled with the required number of samples.
	  * Samples are always ints, later they are converted to the systems
	  * native format (e.g. 16-bit signed).
	  * The Mixer will not call this method if the sound device is muted.
	  *
	  * The default implementation simply calls mixChannels(). If you don't
	  * need anything special you don't need to override this.
	  */
	virtual bool updateBuffer(unsigned length, int* buffer,
	        const EmuTime& start, const EmuDuration& sampDur);

protected:
	/** Abstract method to generate the actual sound data.
	  * @param buffers An array of pointer to buffers. Each buffer must
	  *                be big enough to hold 'num' samples. 
	  * @param num The number of samples.
	  *
	  * This method should fill each buffer with sound data that
	  * corresponds to one channel of the sound device. The same channel
	  * should each time be written to the same buffer (needed for record).
	  *
	  * If a certain channel is muted it is allowed to set the buffer
	  * pointer to NULL. This has exactly the same effect as filling the
	  * buffer completely with zeros, but it can be more efficient.
	  */
	virtual void generateChannels(int** buffers, unsigned num) = 0;

	/** Calls generateChannels() and combines the output to a single
	  * channel.
	  * @param dataOut Output buffer, must be big enough to hold
	  *                'num' samples
	  * @param num The number of samples
	  * @result true iff at least one channel was unmuted
	  */
	bool mixChannels(int* dataOut, unsigned num);

private:
	MSXMixer& mixer;
	const std::string name;
	const std::string description;

	std::auto_ptr<WavWriter> writer[MAX_CHANNELS];

	unsigned inputSampleRate;
	const unsigned numChannels;
	const unsigned stereo;
	unsigned numRecordChannels;
	bool channelMuted[MAX_CHANNELS];
};

} // namespace openmsx

#endif
