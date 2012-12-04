// $Id$

#ifndef MSXMIXER_HH
#define MSXMIXER_HH

#include "Schedulable.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include "DynamicClock.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class SoundDevice;
class Mixer;
class Scheduler;
class MSXCommandController;
class CommandController;
class GlobalSettings;
class ThrottleManager;
class IntegerSetting;
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
	         MSXCommandController& msxCommandController,
	         GlobalSettings& globalSettings);
	virtual ~MSXMixer();

	/**
	 * Use this method to register a given sounddevice.
	 *
	 * While registering, the device its setSampleRate() method is
	 * called (see SoundDevice for more info).
	 * After registration the device its updateBuffer() method is
	 * 'regularly' called (see SoundDevice for more info).
	 */
	void registerSound(SoundDevice& device, double volume,
	                   int balance, unsigned numChannels);

	/**
	 * Every sounddevice must unregister before it is destructed
	 */
	void unregisterSound(SoundDevice& device);

	/**
	 * Use this method to force an 'early' call to all
	 * updateBuffer() methods.
	 */
	void updateStream(EmuTime::param time);

	/** Returns the ratio of emutime-speed per realtime-speed.
	 * In other words how many times faster emutime goes compared to
	 * realtime. This depends on the 'speed' setting but also on whether
	 * we're recording or not (in case of recording we want to generate
	 * sound as if realtime and emutime go at the same speed.
	 */
	double getEffectiveSpeed() const;

	/** If we're recording, we want to emulate sound at 100% emutime speed.
	 * See alsoe getEffectiveSpeed().
	 */
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

	/** Clock that ticks at the exact moment(s) in time that a host sample
	  * should be generated. The current time of this clock is the time of
	  * the last generated sample. The rate of this clock is the same as
	  * the host sample rate.
	  * Note that this rate is not the same as the frequency set with the
	  * 'frequency' setting. Either because the sound driver can't handle
	  * the requested speed or because the 'speed' setting is different
	  * from 100.
	  */
	const DynamicClock& getHostSampleClock() const;

	// Called by AviRecorder
	bool needStereoRecording() const;
	void setRecorder(AviRecorder* recorder);

	// Returns the nominal host sample rate (not adjusted for speed setting)
	unsigned getSampleRate() const;

	SoundDevice* findDevice(string_ref name) const;

	void reInit();

private:
	struct SoundDeviceInfo {
		SoundDeviceInfo();
		SoundDeviceInfo(SoundDeviceInfo&& rhs);
		SoundDeviceInfo& operator=(SoundDeviceInfo&& rhs);

		double defaultVolume;
		std::unique_ptr<IntegerSetting> volumeSetting;
		std::unique_ptr<IntegerSetting> balanceSetting;
		struct ChannelSettings {
			ChannelSettings();
			ChannelSettings(ChannelSettings&& rhs);
			ChannelSettings& operator=(ChannelSettings&& rhs);

			std::unique_ptr<StringSetting> recordSetting;
			std::unique_ptr<BooleanSetting> muteSetting;
		};
		std::vector<ChannelSettings> channelSettings;
		int left1, right1, left2, right2;
	};
	typedef std::map<SoundDevice*, SoundDeviceInfo> Infos;

	void updateVolumeParams(Infos::value_type& p);
	void updateMasterVolume();
	void reschedule();
	void reschedule2();
	void generate(short* buffer, EmuTime::param time, unsigned samples);

	// Schedulable
	void executeUntil(EmuTime::param time, int userData);

	// Observer<Setting>
	virtual void update(const Setting& setting);
	// Observer<ThrottleManager>
	virtual void update(const ThrottleManager& throttleManager);

	void changeRecordSetting(const Setting& setting);
	void changeMuteSetting(const Setting& setting);

	unsigned fragmentSize;
	unsigned hostSampleRate; // requested freq by sound driver,
	                         // not compensated for speed

	Infos infos;

	Mixer& mixer;
	CommandController& commandController;

	IntegerSetting& masterVolume;
	IntegerSetting& speedSetting;
	ThrottleManager& throttleManager;

	DynamicClock prevTime;

	friend class SoundDeviceInfoTopic;
	const std::unique_ptr<SoundDeviceInfoTopic> soundDeviceInfo;

	AviRecorder* recorder;
	unsigned synchronousCounter;

	unsigned muteCount;
	int prevLeft, prevRight;
	int outLeft, outRight;
};

} // namespace openmsx

#endif
