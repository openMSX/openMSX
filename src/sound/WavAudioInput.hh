// $Id$

#ifndef WAVAUDIOINPUT_HH
#define WAVAUDIOINPUT_HH

#include "AudioInputDevice.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;
class Setting;
class WavData;

class WavAudioInput : public AudioInputDevice, private Observer<Setting>
{
public:
	explicit WavAudioInput(CommandController& commandController);
	virtual ~WavAudioInput();

	// AudioInputDevice
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual short readSample(const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void loadWave();
	void update(const Setting& setting);

	const std::auto_ptr<FilenameSetting> audioInputFilenameSetting;

	std::auto_ptr<WavData> wav;
	EmuTime reference;
	bool plugged;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, WavAudioInput, "WavAudioInput");

} // namespace openmsx

#endif
