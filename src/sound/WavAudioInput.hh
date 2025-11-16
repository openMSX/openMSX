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
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] int16_t readSample(EmuTime time) override;

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
