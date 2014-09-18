#ifndef WAVAUDIOINPUT_HH
#define WAVAUDIOINPUT_HH

#include "AudioInputDevice.hh"
#include "WavData.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;
class Setting;

class WavAudioInput final : public AudioInputDevice, private Observer<Setting>
{
public:
	explicit WavAudioInput(CommandController& commandController);
	virtual ~WavAudioInput();

	// AudioInputDevice
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual short readSample(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void loadWave();
	void update(const Setting& setting);

	const std::unique_ptr<FilenameSetting> audioInputFilenameSetting;

	WavData wav;
	EmuTime reference;
};

} // namespace openmsx

#endif
