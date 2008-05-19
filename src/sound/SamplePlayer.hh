// $Id$

#ifndef SAMPLEPLAYER_HH
#define SAMPLEPLAYER_HH

#include "SoundDevice.hh"
#include "Resample.hh"

namespace openmsx {

class MSXMotherBoard;

class SamplePlayer : public SoundDevice, private Resample
{
public:
	SamplePlayer(MSXMotherBoard& motherBoard, const std::string& name,
	             const std::string& desc, const XMLElement& config);
	~SamplePlayer();

	void reset();

	/** Start playing a (new) sample.
	 * @param buffer The sample data
	 * @param bufferSize Size of the buffer in samples
	 * @param bits Bits per sample, must be either 8 or 16.
	 *              8 bit format is unsigned
	 *             16 bit format is signed
	 * @param freq The sample frequency. Preferably all samples should
	 *             have the sample frequency, because when switching
	 *             playback frequency some of the old samples can be
	 *             dropped.
	 */
	void play(const void* buffer, unsigned bufferSize,
	          unsigned bits, unsigned freq);

	/** Plays the given data if there's no sample playing, otherwise sets
	 * the data to play when the current sample is finished playing (if
	 * repeat is enabled).
	 * @param buffer The sample data
	 * @param bufferSize Size of the buffer in samples
	 * @param bits Bits per sample, must be either 8 or 16.
	 *              8 bit format is unsigned
	 *             16 bit format is signed
	 * @param freq The sample frequency. Preferably all samples should
	 *             have the sample frequency, because when switching
	 *             playback frequency some of the old samples can be
	 *             dropped.
	 */
	void setRepeatDataOrPlay(const void* buffer, unsigned bufferSize,
	          unsigned bits, unsigned freq);

	/** Is there currently playing a sample. */
	bool isPlaying() const;

	/** Set repeat mode
	 * @param enabled true to repeat after playing, false to stop
	 */
	void setRepeat(bool enabled);

private:
	inline int getSample(unsigned index);

	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	const void* sampBuf;
	unsigned inFreq;
	unsigned outFreq;
	unsigned index;
	unsigned bufferSize;
	bool playing;
	bool bits8;

	bool repeat;

	const void* nextBuffer;
	unsigned nextBufferSize;
	unsigned nextBits;
	unsigned nextFreq;
};

} // namespace openmsx

#endif
