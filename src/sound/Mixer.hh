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
class WavWriter;
class Setting;
class SoundlogCommand;

class Mixer : private Observer<Setting>, private noncopyable
{
public:
	Mixer(CommandController& commandController);
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

	/**
	 * This methods (un)locks the audio thread.
	 * You can use this method to delay the call to the SoundDevices
	 * updateBuffer() method. For example, this is usefull if
	 * you are updating a lot of registers and you don't want the
	 * half updated set being used to produce sound
	 */
	void lock();
	void unlock();

	/** Upload new sample data
	 */
	double uploadBuffer(MSXMixer& msxMixer, short* buffer, unsigned len);

	IntegerSetting& getMasterVolume() const;

	// Called by (some) SoundDrivers
	
	/** Emergency callback: generate extra samples.
	 * This is called when a bufferrun is about to occur. Only the
	 * SDLSoundDriver uses this method ATM.
	 * Note: this method runs in the audio thread (for SDLSoundDriver)
	 */
	void bufferUnderRun(short* buffer, unsigned samples);

private:
	void reloadDriver();
	void muteHelper();
	void writeWaveData(short* buffer, unsigned samples);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	typedef std::vector<MSXMixer*> MSXMixers;
	MSXMixers msxMixers;

	std::auto_ptr<SoundDriver> driver;
	int muteCount;
	CommandController& commandController;

	std::auto_ptr<BooleanSetting> muteSetting;
	std::auto_ptr<IntegerSetting> masterVolume;
	std::auto_ptr<IntegerSetting> frequencySetting;
	std::auto_ptr<IntegerSetting> samplesSetting;
	enum SoundDriverType { SND_NULL, SND_SDL, SND_DIRECTX };
	std::auto_ptr<EnumSetting<SoundDriverType> > soundDriverSetting;
	BooleanSetting& pauseSetting;

	std::auto_ptr<WavWriter> wavWriter;

	friend class SoundlogCommand;
	const std::auto_ptr<SoundlogCommand> soundlogCommand;
};

} // namespace openmsx

#endif
