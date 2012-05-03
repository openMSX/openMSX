#ifndef SAMPLEPLAYER_HH
#define SAMPLEPLAYER_HH

#include "ResampledSoundDevice.hh"
#include "WavData.hh"
#include <vector>

namespace openmsx {

class SamplePlayer final : public ResampledSoundDevice
{
public:
	SamplePlayer(const std::string& name, const std::string& desc,
	             const DeviceConfig& config,
	             const std::string& samplesBaseName, unsigned numSamples,
	             const std::string& alternativeName = {});
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
	void stopRepeat() { nextSampleNum = unsigned(-1); }

	/** Is there currently playing a sample. */
	bool isPlaying() const { return currentSampleNum != unsigned(-1); }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	inline int getSample(unsigned index);
	void setWavParams();
	void doRepeat();

	// SoundDevice
	void generateChannels(int** bufs, unsigned num) override;

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
