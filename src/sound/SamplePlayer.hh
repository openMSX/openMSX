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

	/** Keep on repeating the given sample data.
	 * If there is already a sample playing, that sample is still
	 * finished. If there was no sample playing, the given sample
	 * immediatly starts playing.
	 * Parameters are the same as for the play() method.
	 * @see stopRepeat()
	 */
	void repeat(const void* buffer, unsigned bufferSize,
	            unsigned bits, unsigned freq);

	/** Stop repeat mode.
	 * The currently playing sample will still be finished, but won't
	 * be started.
	 * @see repeat()
	 */
	void stopRepeat();

	/** Is there currently playing a sample. */
	bool isPlaying() const;

private:
	inline int getSample(unsigned index);
	void doRepeat();

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

	const void* nextBuffer;
	unsigned nextBufferSize;
	unsigned nextBits;
	unsigned nextFreq;
};

} // namespace openmsx

#endif
