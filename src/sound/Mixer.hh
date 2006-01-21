// $Id$

#ifndef MIXER_HH
#define MIXER_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class EmuTime;
class EmuDuration;
class SoundDevice;
class SoundDriver;
class Scheduler;
class CommandController;
class IntegerSetting;
class BooleanSetting;
template <typename T> class EnumSetting;
class WavWriter;
class Setting;
class SoundlogCommand;
class SoundDeviceInfoTopic;

class Mixer : private Observer<Setting>, private noncopyable
{
public:
	static const int MAX_VOLUME = 32767;
	enum ChannelMode {
		MONO, MONO_LEFT, MONO_RIGHT, STEREO, OFF, NB_MODES
	};

	Mixer(Scheduler& scheduler, CommandController& commandController);
	virtual ~Mixer();

	/**
	 * Use this method to register a given sounddevice.
	 *
	 * While registering, the device its setSampleRate() method is
	 * called (see SoundDevice for more info).
	 * After registration the device its updateBuffer() method is
	 * 'regularly' called (see SoundDevice for more info).
	 */
	void registerSound(SoundDevice& device, short volume, ChannelMode mode);

	/**
	 * Every sounddevice must unregister before it is destructed
	 */
	void unregisterSound(SoundDevice& device);

	/**
	 * Use this method to force an 'early' call to all
	 * updateBuffer() methods.
	 */
	void updateStream(const EmuTime& time);

	/**
	 * This methods (un)locks the audio thread.
	 * You can use this method to delay the call to the SoundDevices
	 * updateBuffer() method. For example, this is usefull if
	 * you are updating a lot of registers and you don't want the
	 * half updated set being used to produce sound
	 */
	void lock();
	void unlock();

	/**
	 * This methods (un)mute the sound.
	 * These methods may be called multiple times, as long as
	 * you never call unmute() more than mute()
	 */
	void mute();
	void unmute();

	void generate(short* buffer, unsigned samples, const EmuTime& start,
	              const EmuDuration& sampDur);

private:
	void openSound();
	void reopenSound();
	void muteHelper();

	void updateMasterVolume(int masterVolume);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	SoundDevice* getSoundDevice(const std::string& name);

	std::auto_ptr<SoundDriver> driver;
	int muteCount;

	struct SoundDeviceInfo {
		ChannelMode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode>* modeSetting;
	};
	typedef std::map<SoundDevice*, SoundDeviceInfo> Infos;
	Infos infos;

	std::vector<SoundDevice*> devices[NB_MODES];
	std::vector<int*> buffers;

	Scheduler& scheduler;
	CommandController& commandController;

	std::auto_ptr<BooleanSetting> muteSetting;
	std::auto_ptr<IntegerSetting> masterVolume;
	std::auto_ptr<IntegerSetting> frequencySetting;
	std::auto_ptr<IntegerSetting> samplesSetting;
	enum SoundDriverType { SND_NULL, SND_SDL, SND_DIRECTX };
	std::auto_ptr<EnumSetting<SoundDriverType> > soundDriverSetting;
	BooleanSetting& pauseSetting;
	bool handlingUpdate;

	std::auto_ptr<WavWriter> wavWriter;

	int prevLeft, outLeft;
	int prevRight, outRight;

	friend class SoundlogCommand;
	friend class SoundDeviceInfoTopic;
	const std::auto_ptr<SoundlogCommand> soundlogCommand;
	const std::auto_ptr<SoundDeviceInfoTopic> soundDeviceInfo;
};

} // namespace openmsx

#endif
