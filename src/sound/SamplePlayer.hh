#ifndef SAMPLEPLAYER_HH
#define SAMPLEPLAYER_HH

#include "ResampledSoundDevice.hh"
#include "WavData.hh"
#include "dynarray.hh"

namespace openmsx {

class SamplePlayer final : public ResampledSoundDevice
{
public:
	SamplePlayer(const std::string& name, static_string_view desc,
	             const DeviceConfig& config,
	             std::string_view samplesBaseName, unsigned numSamples,
	             std::string_view alternativeName = {});
	~SamplePlayer();

	void reset();

	/** Start playing a (new) sample.
	 */
	void play(unsigned sampleNum);

	/** Keep on repeating the given sample data.
	 * If there is already a sample playing, that sample is still
	 * finished. If there was no sample playing, the given sample
	 * immediately starts playing.
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
	[[nodiscard]] bool isPlaying() const { return currentSampleNum != unsigned(-1); }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setWavParams();
	void doRepeat();

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;

private:
	const dynarray<WavData> samples;

	unsigned index;
	unsigned bufferSize;
	unsigned currentSampleNum;
	unsigned nextSampleNum;
};

} // namespace openmsx

#endif
