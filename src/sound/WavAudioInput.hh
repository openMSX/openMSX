#ifndef WAVAUDIOINPUT_HH
#define WAVAUDIOINPUT_HH

#include "AudioInputDevice.hh"
#include "EmuTime.hh"
#include "FilenameSetting.hh"
#include "Observer.hh"
#include "WavData.hh"

namespace openmsx {

class CommandController;

class WavAudioInput final : public AudioInputDevice, private Observer<Setting>
{
public:
	explicit WavAudioInput(CommandController& commandController);
	~WavAudioInput() override;

	// AudioInputDevice
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] int16_t readSample(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void loadWave();
	void update(const Setting& setting) noexcept override;

private:
	FilenameSetting audioInputFilenameSetting;
	WavData wav;
	EmuTime reference = EmuTime::zero();
};

} // namespace openmsx

#endif
