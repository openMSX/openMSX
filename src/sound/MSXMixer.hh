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
class StringSetting;
class BooleanSetting;
class Setting;
class SoundDeviceInfoTopic;
class AviRecorder;

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
	                   ChannelMode::Mode mode, unsigned numChannels);

	/**
	 * Every sounddevice must unregister before it is destructed
	 */
	void unregisterSound(SoundDevice& device);

	/**
	 * Use this method to force an 'early' call to all
	 * updateBuffer() methods.
	 */
	void updateStream(const EmuTime& time);

	// TODO
	void setSynchronousMode(bool synchronous);

	/** TODO
	 * This methods (un)mute the sound.
	 * These methods may be called multiple times, as long as
	 * you never call unmute() more than mute()
	 */
	void mute();
	void unmute();

	// Called by Mixer or SoundDriver
	
	/** Set new fragment size and sample frequency.
	 * A fragment size of zero means the Mixer is muted.
	 */
	void setMixerParams(unsigned fragmentSize, unsigned sampleRate);

	// Called by AviRecorder
	
	void setRecorder(AviRecorder* recorder);
	unsigned getSampleRate() const;
	
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

	void changeRecordSetting(const Setting& setting);
	void changeMuteSetting(const Setting& setting);

	unsigned sampleRate;
	unsigned fragmentSize;

	struct SoundDeviceInfo {
		ChannelMode::Mode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode::Mode>* modeSetting;
		struct ChannelSettings {
			StringSetting* recordSetting;
			BooleanSetting* muteSetting;
		};
		std::vector<ChannelSettings> channelSettings;
	};
	typedef std::map<SoundDevice*, SoundDeviceInfo> Infos;
	Infos infos;

	std::vector<SoundDevice*> devices[ChannelMode::NB_MODES];
	std::vector<int> mixBuffer;

	Mixer& mixer;
	MSXCommandController& msxCommandController;

	IntegerSetting& masterVolume;
	IntegerSetting& speedSetting;
	ThrottleManager& throttleManager;

	unsigned muteCount;
	int prevLeft, prevRight;
	int outLeft, outRight;

	EmuTime prevTime;
	EmuDuration interval1; ///<  (Estimated) duration for one sample
	EmuDuration interval1min;
	EmuDuration interval1max;

	friend class SoundDeviceInfoTopic;
	const std::auto_ptr<SoundDeviceInfoTopic> soundDeviceInfo;

	AviRecorder* recorder;
	unsigned synchronousCounter;
};

} // namespace openmsx

#endif
