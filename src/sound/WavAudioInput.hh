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
	~WavAudioInput();

	// AudioInputDevice
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	short readSample(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void loadWave();
	void update(const Setting& setting) override;

	const std::unique_ptr<FilenameSetting> audioInputFilenameSetting;

	WavData wav;
	EmuTime reference;
};

} // namespace openmsx

#endif
