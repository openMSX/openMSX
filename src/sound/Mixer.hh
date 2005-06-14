// $Id$

#ifndef MIXER_HH
#define MIXER_HH

#include <vector>
#include <map>
#include <memory>
#include "EmuTime.hh"
#include "SettingListener.hh"
#include "InfoTopic.hh"
#include "Command.hh"

namespace openmsx {

class SoundDevice;
class SoundDriver;
class CliComm;
class InfoCommand;
class VolumeSetting;
class IntegerSetting;
class BooleanSetting;
template <typename T> class EnumSetting;

class Mixer : private SettingListener
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

	void generate(short* buffer, unsigned samples);

private:
	Mixer();
	virtual ~Mixer();

	void openSound();
	void reopenSound();
	void muteHelper();

	void updateMasterVolume(int masterVolume);

	void startSoundLogging(const std::string& filename);
	void endSoundLogging();

	// SettingListener
	virtual void update(const Setting* setting);

	SoundDevice* getSoundDevice(const std::string& name);

	std::auto_ptr<SoundDriver> driver;
	int muteCount;

	struct SoundDeviceInfo {
		ChannelMode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode>* modeSetting;
	};
	std::map<SoundDevice*, SoundDeviceInfo> infos;

	std::vector<SoundDevice*> devices[NB_MODES];
	std::vector<int*> buffers;

	CliComm& output;
	InfoCommand& infoCommand;

	std::auto_ptr<BooleanSetting> muteSetting;
	std::auto_ptr<IntegerSetting> masterVolume;
	std::auto_ptr<IntegerSetting> frequencySetting;
	std::auto_ptr<IntegerSetting> samplesSetting;
	enum SoundDriverType { SND_NULL, SND_SDL, SND_DIRECTX };
	std::auto_ptr<EnumSetting<SoundDriverType> > soundDriverSetting;
	BooleanSetting& pauseSetting;
	bool handlingUpdate;

	FILE* wavfp;
	uint32 nofWavBytes;

	int prevLeft, outLeft;
	int prevRight, outRight;

	// Commands
	class SoundlogCommand : public SimpleCommand {
	public:
		SoundlogCommand(Mixer& outer);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		std::string stopSoundLogging(const std::vector<std::string>& tokens);
		std::string startSoundLogging(const std::vector<std::string>& tokens);
		std::string toggleSoundLogging(const std::vector<std::string>& tokens);
		std::string getFileName();
		Mixer& outer;
	} soundlogCommand;

	// Info
	class SoundDeviceInfoTopic : public InfoTopic {
	public:
		SoundDeviceInfoTopic(Mixer& outer);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		Mixer& outer;
	} soundDeviceInfo;
};

} // namespace openmsx

#endif
