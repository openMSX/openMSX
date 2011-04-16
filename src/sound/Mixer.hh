// $Id$

#ifndef MIXER_HH
#define MIXER_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class SoundDriver;
class CommandController;
class MSXMixer;
class IntegerSetting;
class BooleanSetting;
template <typename T> class EnumSetting;
class Setting;

class Mixer : private Observer<Setting>, private noncopyable
{
public:
	explicit Mixer(CommandController& commandController);
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
	double uploadBuffer(MSXMixer& msxMixer, short* buffer, unsigned len);

	IntegerSetting& getMasterVolume() const;

private:
	void reloadDriver();
	void muteHelper();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	typedef std::vector<MSXMixer*> MSXMixers;
	MSXMixers msxMixers;

	std::auto_ptr<SoundDriver> driver;
	CommandController& commandController;

	const std::auto_ptr<BooleanSetting> muteSetting;
	const std::auto_ptr<IntegerSetting> masterVolume;
	const std::auto_ptr<IntegerSetting> frequencySetting;
	const std::auto_ptr<IntegerSetting> samplesSetting;
	enum SoundDriverType { SND_NULL, SND_SDL, SND_DIRECTX, SND_LIBAO };
	std::auto_ptr<EnumSetting<SoundDriverType> > soundDriverSetting;

	int muteCount;
};

} // namespace openmsx

#endif
