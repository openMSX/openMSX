// $Id$

#ifndef SAMPLEPLAYER_HH
#define SAMPLEPLAYER_HH

#include "ResampledSoundDevice.hh"
#include "WavData.hh"
#include <memory>
#include <vector>

namespace openmsx {

class MSXMotherBoard;

class SamplePlayer : public ResampledSoundDevice
{
public:
	SamplePlayer(const std::string& name, const std::string& desc,
	             const DeviceConfig& config,
	             const std::string& samplesBaseName, unsigned numSamples,
	             const std::string& alternativeName = "");
	~SamplePlayer();

	void reset();

	/** Start playing a (new) sample.
	 */
	void play(unsigned sampleNum);

	/** Keep on repeating the given sample data.
	 * If there is already a sample playing, that sample is still
	 * finished. If there was no sample playing, the given sample
	 * immediatly starts playing.
	 * Parameters are the same as for the play() method.
	 * @see stopRepeat()
	 */
	void repeat(unsigned sampleNum);

	/** Stop repeat mode.
	 * The currently playing sample will still be finished, but won't
	 * be started.
	 * @see repeat()
	 */
	void stopRepeat();

	/** Is there currently playing a sample. */
	bool isPlaying() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	inline int getSample(unsigned index);
	void setWavParams();
	void doRepeat();

	// SoundDevice
	virtual void generateChannels(int** bufs, unsigned num);

	std::vector<WavData> samples;

	const void* sampBuf;
	unsigned index;
	unsigned bufferSize;
	unsigned currentSampleNum;
	unsigned nextSampleNum;
	bool bits8;
};

} // namespace openmsx

#endif
