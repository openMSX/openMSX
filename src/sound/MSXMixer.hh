#ifndef MSXMIXER_HH
#define MSXMIXER_HH

#include "Schedulable.hh"
#include "Observer.hh"
#include "InfoTopic.hh"
#include "EmuTime.hh"
#include "DynamicClock.hh"
#include <vector>
#include <memory>

namespace openmsx {

class SoundDevice;
class Mixer;
class MSXMotherBoard;
class MSXCommandController;
class GlobalSettings;
class SpeedManager;
class ThrottleManager;
class IntegerSetting;
class StringSetting;
class BooleanSetting;
class Setting;
class AviRecorder;

class MSXMixer final : private Schedulable, private Observer<Setting>
                     , private Observer<SpeedManager>
                     , private Observer<ThrottleManager>
{
public:
	// See SoundDevice::getAmplificationFactor()
	// and MSXMixer::updateVolumeParams()
	static constexpr int AMP_BITS = 9;

public:
	MSXMixer(const MSXMixer&) = delete;
	MSXMixer& operator=(const MSXMixer&) = delete;

	MSXMixer(Mixer& mixer, MSXMotherBoard& motherBoard,
	         GlobalSettings& globalSettings);
	~MSXMixer();

	/**
	 * Use this method to register a given sounddevice.
	 *
	 * While registering, the device its setSampleRate() method is
	 * called (see SoundDevice for more info).
	 * After registration the device its updateBuffer() method is
	 * 'regularly' called (see SoundDevice for more info).
	 */
	void registerSound(SoundDevice& device, float volume,
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

	/**
	 * Used by SoundDevice::setSoftwareVolume()
	 */
	void updateSoftwareVolume(SoundDevice& device);

	/** Returns the ratio of emutime-speed per realtime-speed.
	 * In other words how many times faster emutime goes compared to
	 * realtime. This depends on the 'speed' setting but also on whether
	 * we're recording or not (in case of recording we want to generate
	 * sound as if realtime and emutime go at the same speed.
	 */
	[[nodiscard]] double getEffectiveSpeed() const;

	/** If we're recording, we want to emulate sound at 100% emutime speed.
	 * See also getEffectiveSpeed().
	 */
	void setSynchronousMode(bool synchronous);
	[[nodiscard]] bool isSynchronousMode() const { return synchronousCounter != 0; }

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
	[[nodiscard]] const DynamicClock& getHostSampleClock() const { return prevTime; }

	// Called by AviRecorder
	[[nodiscard]] bool needStereoRecording() const;
	void setRecorder(AviRecorder* recorder);

	// Returns the nominal host sample rate (not adjusted for speed setting)
	[[nodiscard]] unsigned getSampleRate() const { return hostSampleRate; }

	[[nodiscard]] SoundDevice* findDevice(std::string_view name) const;

	void reInit();

private:
	struct SoundDeviceInfo {
		SoundDevice* device;
		float defaultVolume;
		std::unique_ptr<IntegerSetting> volumeSetting;
		std::unique_ptr<IntegerSetting> balanceSetting;
		struct ChannelSettings {
			std::unique_ptr<StringSetting> recordSetting;
			std::unique_ptr<BooleanSetting> muteSetting;
		};
		std::vector<ChannelSettings> channelSettings;
		float left1, right1, left2, right2;
	};

	void updateVolumeParams(SoundDeviceInfo& info);
	void updateMasterVolume();
	void reschedule();
	void reschedule2();
	void generate(float* output, EmuTime::param time, unsigned samples);

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;
	// Observer<SpeedManager>
	void update(const SpeedManager& speedManager) noexcept override;
	// Observer<ThrottleManager>
	void update(const ThrottleManager& throttleManager) noexcept override;

	void changeRecordSetting(const Setting& setting);
	void changeMuteSetting(const Setting& setting);

private:
	unsigned fragmentSize;
	unsigned hostSampleRate; // requested freq by sound driver,
	                         // not compensated for speed

	std::vector<SoundDeviceInfo> infos;

	Mixer& mixer;
	MSXMotherBoard& motherBoard;
	MSXCommandController& commandController;

	IntegerSetting& masterVolume;
	SpeedManager& speedManager;
	ThrottleManager& throttleManager;

	DynamicClock prevTime;

	struct SoundDeviceInfoTopic final : InfoTopic {
		explicit SoundDeviceInfoTopic(InfoCommand& machineInfoCommand);
		void execute(span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} soundDeviceInfo;

	AviRecorder* recorder;
	unsigned synchronousCounter;

	unsigned muteCount;
	float tl0, tr0; // internal DC-filter state
};

} // namespace openmsx

#endif
