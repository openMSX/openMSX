// $Id$

#ifndef MSXMIXER_HH
#define MSXMIXER_HH

#include "ChannelMode.hh"
#include "Schedulable.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include "EmuDuration.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class SoundDevice;
class Mixer;
class Scheduler;
class MSXCommandController;
class ThrottleManager;
class IntegerSetting;
template <typename T> class EnumSetting;
class Setting;
class SoundDeviceInfoTopic;

class MSXMixer : private Schedulable, private Observer<Setting>
               , private Observer<ThrottleManager>
{
public:
	MSXMixer(Mixer& mixer, Scheduler& scheduler,
	         MSXCommandController& msxCommandController);
	virtual ~MSXMixer();

	/**
	 * Use this method to register a given sounddevice.
	 *
	 * While registering, the device its setSampleRate() method is
	 * called (see SoundDevice for more info).
	 * After registration the device its updateBuffer() method is
	 * 'regularly' called (see SoundDevice for more info).
	 */
	void registerSound(SoundDevice& device, short volume,
	                   ChannelMode::Mode mode);

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

	/** TODO
	 * This methods (un)mute the sound.
	 * These methods may be called multiple times, as long as
	 * you never call unmute() more than mute()
	 */
	void mute();
	void unmute();

	// Called by Mixer or SoundDriver
	
	/** Set new buffer size and sample frequency.
	 * A buffer size of zero means the Mixer is muted.
	 */
	void setMixerParams(unsigned bufferSize, unsigned sampleRate);
	
	/** Emergency callback: generate extra samples.
	 * See Mixer::bufferUnderRun()
	 * Note: this method runs in the audio thread (for SDLSoundDriver)
	 */
	void bufferUnderRun(short* buffer, unsigned samples);

private:
	void updateMasterVolume(int masterVolume);
	SoundDevice* getSoundDevice(const std::string& name);
	void reInit();
	void updateStream2(const EmuTime& time);
	void generate(short* buffer, unsigned samples, const EmuTime& start,
	              const EmuDuration& sampDur);

	// Schedulable
	void executeUntil(const EmuTime& time, int userData);
	const std::string& schedName() const;	

	// Observer<Setting>
	virtual void update(const Setting& setting);
	// Observer<ThrottleManager>
	virtual void update(const ThrottleManager& throttleManager);

	unsigned sampleRate;
	unsigned bufferSize;

	struct SoundDeviceInfo {
		ChannelMode::Mode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode::Mode>* modeSetting;
	};
	typedef std::map<SoundDevice*, SoundDeviceInfo> Infos;
	Infos infos;

	std::vector<SoundDevice*> devices[ChannelMode::NB_MODES];
	std::vector<int*> buffers;

	Mixer& mixer;
	MSXCommandController& msxCommandController;

	IntegerSetting& masterVolume;
	IntegerSetting& speedSetting;
	ThrottleManager& throttleManager;

	unsigned muteCount;
	int prevLeft, prevRight;
	int outLeft, outRight;

	EmuTime prevTime;
	EmuDuration interval1;
	EmuDuration intervalAverage;

	friend class SoundDeviceInfoTopic;
	const std::auto_ptr<SoundDeviceInfoTopic> soundDeviceInfo;
};

} // namespace openmsx

#endif
