#ifndef MIXER_HH
#define MIXER_HH

#include "Observer.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include <vector>
#include <memory>

namespace openmsx {

class SoundDriver;
class Reactor;
class CommandController;
class MSXMixer;

struct StereoFloat {
	float left  = 0.0f;
	float right = 0.0f;
};

class Mixer final : private Observer<Setting>
{
public:
	enum SoundDriverType { SND_NULL, SND_SDL };

	Mixer(Reactor& reactor, CommandController& commandController);
	~Mixer();

	/** Register per-machine mixer
	 */
	void registerMixer(MSXMixer& mixer);

	/** Unregister per-machine mixer
	 */
	void unregisterMixer(MSXMixer& mixer);

	/**
	 * This methods (un)mute the sound.
	 * These methods may be called multiple times, as long as
	 * you never call unmute() more than mute()
	 */
	void mute();
	void unmute();

	// Called by MSXMixer

	/** Upload new sample data
	 */
	void uploadBuffer(MSXMixer& msxMixer, std::span<const StereoFloat> buffer);

	[[nodiscard]] IntegerSetting& getMasterVolume() { return masterVolume; }

private:
	void reloadDriver();
	void muteHelper();

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	std::vector<MSXMixer*> msxMixers; // unordered

	std::unique_ptr<SoundDriver> driver;
	Reactor& reactor;
	CommandController& commandController;

	EnumSetting<SoundDriverType> soundDriverSetting;
	BooleanSetting muteSetting;
	IntegerSetting masterVolume;
	IntegerSetting frequencySetting;
	IntegerSetting samplesSetting;

	int muteCount = 0;
};

} // namespace openmsx

#endif
