#ifndef MIXER_HH
#define MIXER_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class SoundDriver;
class Reactor;
class CommandController;
class MSXMixer;
class IntegerSetting;
class BooleanSetting;
template <typename T> class EnumSetting;
class Setting;

class Mixer : private Observer<Setting>, private noncopyable
{
public:
	Mixer(Reactor& reactor, CommandController& commandController);
	virtual ~Mixer();

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
	void uploadBuffer(MSXMixer& msxMixer, short* buffer, unsigned len);

	IntegerSetting& getMasterVolume() const;

private:
	void reloadDriver();
	void muteHelper();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	std::vector<MSXMixer*> msxMixers;

	std::unique_ptr<SoundDriver> driver;
	Reactor& reactor;
	CommandController& commandController;

	const std::unique_ptr<BooleanSetting> muteSetting;
	const std::unique_ptr<IntegerSetting> masterVolume;
	const std::unique_ptr<IntegerSetting> frequencySetting;
	const std::unique_ptr<IntegerSetting> samplesSetting;
	enum SoundDriverType { SND_NULL, SND_SDL, SND_DIRECTX };
	std::unique_ptr<EnumSetting<SoundDriverType>> soundDriverSetting;

	int muteCount;
};

} // namespace openmsx

#endif
