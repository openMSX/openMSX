// $Id$

#ifndef SOUNDDEVICE_HH
#define SOUNDDEVICE_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include <string>
#include <set>
#include <memory>

namespace openmsx {

class MSXMixer;
class XMLElement;
class EmuDuration;
class Wav16Writer;
class Filename;

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

	/** Get extra amplification factor for this device.
	  * Normally the outputBuffer() method should scale the output to
	  * the range [-32768,32768]. Some devices can be emulated slightly
	  * faster to produce another output range. In later stages the output
	  * is anyway still multiplied by some factor. This method tells which
	  * factor should be used to scale the output to the correct range.
	  * The default implementation returns 1.
	  */
	virtual int getAmplificationFactor() const;

	void recordChannel(unsigned channel, const Filename& filename);
	void muteChannel  (unsigned channel, bool muted);

protected:
	/** Constructor.
	  * @param mixer The Mixer object
	  * @param name Name for this device, will be made unique
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
	void updateStream(EmuTime::param time);

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
	  *
	  * Note: To enable various optimizations (like SSE), this method can
	  * fill the output buffer with up to 3 extra samples. Those extra
	  * samples should be ignored, though the caller must make sure the
	  * buffer has enough space to hold them.
	  */
	virtual bool updateBuffer(unsigned length, int* buffer,
	        EmuTime::param start, EmuDuration::param sampDur) = 0;

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
	  *
	  * Note: To enable various optimizations (like SSE), this method can
	  * fill the output buffer with up to 3 extra samples. Those extra
	  * samples should be ignored, though the caller must make sure the
	  * buffer has enough space to hold them.
	  */
	bool mixChannels(int* dataOut, unsigned num);

private:
	MSXMixer& mixer;
	const std::string name;
	const std::string description;

	std::auto_ptr<Wav16Writer> writer[MAX_CHANNELS];

	unsigned inputSampleRate;
	const unsigned numChannels;
	const unsigned stereo;
	bool balanceCenter;
	unsigned numRecordChannels;
	int channelBalance[MAX_CHANNELS];
	bool channelMuted[MAX_CHANNELS];
};

} // namespace openmsx

#endif
