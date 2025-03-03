#ifndef MIXER_HH
#define MIXER_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"

#include "Observer.hh"

#include <cstdint>
#include <memory>
#include <vector>

namespace openmsx {

class SoundDriver;
class Reactor;
class CommandController;
class MSXMixer;

struct StereoFloat {
	// Note: important to keep this uninitialized, we have large arrays of
	// these objects and needlessly initializing those is expensive
	float left;
	float right;
};

class Mixer final : private Observer<Setting>
{
public:
	enum class SoundDriverType : uint8_t { NONE, SDL };

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
	[[nodiscard]] BooleanSetting& getMuteSetting() { return muteSetting; }

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
