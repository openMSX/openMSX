// $Id$

#ifndef MIXER_HH
#define MIXER_HH

#include <SDL.h>
#include <vector>
#include <map>
#include <memory>
#include "EmuTime.hh"
#include "SettingListener.hh"
#include "InfoTopic.hh"
#include "Schedulable.hh"

namespace openmsx {

class SoundDevice;
class MSXCPU;
class SettingsConfig;
class RealTime;
class CliCommOutput;
class InfoCommand;
class VolumeSetting;
class IntegerSetting;
class BooleanSetting;
template <typename T> class EnumSetting;

class Mixer : private Schedulable, private SettingListener
{
public:
	static const int MAX_VOLUME = 32767;
	enum ChannelMode {
		MONO, MONO_LEFT, MONO_RIGHT, STEREO, OFF, NB_MODES
	};

	static Mixer& instance();

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
	
private:
	Mixer();
	virtual ~Mixer();

	void openSound();
	void closeSound();
	void reopenSound();
	
	void updtStrm(unsigned samples);
	void updtStrm2(unsigned samples);
	static void audioCallbackHelper(void* userdata, Uint8* stream, int len);
	void audioCallback(short* stream, unsigned len);
	void muteHelper();
	
	void updateMasterVolume(int masterVolume);
	void reInit();

	void startRecording();
	void endRecording();

	// SettingListener
	virtual void update(const Setting* setting);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;
	
	SoundDevice* getSoundDevice(const std::string& name);

	bool init;
	int muteCount;

	struct SoundDeviceInfo {
		ChannelMode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode>* modeSetting;
	};
	std::map<SoundDevice*, SoundDeviceInfo> infos;

	SDL_AudioSpec audioSpec;
	std::vector<SoundDevice*> devices[NB_MODES];
	std::vector<int*> buffers;

	short* mixBuffer;
	unsigned bufferSize;
	unsigned readPtr, writePtr;
	EmuTime prevTime;
	EmuDuration interval1;
	EmuDuration intervalAverage;

	MSXCPU& cpu;
	RealTime& realTime;
	SettingsConfig& settingsConfig;
	CliCommOutput& output;
	InfoCommand& infoCommand;

	std::auto_ptr<BooleanSetting> muteSetting;
	std::auto_ptr<IntegerSetting> masterVolume;
	std::auto_ptr<IntegerSetting> frequencySetting;
	std::auto_ptr<IntegerSetting> samplesSetting;
	BooleanSetting& pauseSetting;
	IntegerSetting& speedSetting;
	BooleanSetting& throttleSetting;
	bool handlingUpdate;
	
	FILE* wavfp;
	uint32 nofWavBytes;

	int prevLeft, outLeft;
	int prevRight, outRight;

	class SoundDeviceInfoTopic : public InfoTopic {
	public:
		SoundDeviceInfoTopic(Mixer& parent);
		virtual void execute(const std::vector<CommandArgument>& tokens,
		                     CommandArgument& result) const;
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		Mixer& parent;
	} soundDeviceInfo;
};

} // namespace openmsx

#endif
