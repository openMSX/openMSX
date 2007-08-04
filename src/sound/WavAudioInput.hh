// $Id$

#ifndef WAVAUDIOINPUT_HH
#define WAVAUDIOINPUT_HH

#include "AudioInputDevice.hh"
#include "Observer.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;
class Setting;

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

private:
	void freeWave();
	void loadWave();
	void update(const Setting& setting);

	const std::auto_ptr<FilenameSetting> audioInputFilenameSetting;

	EmuTime reference;
	short* buffer;
	int length;
	int freq;
	bool plugged;
};

} // namespace openmsx

#endif
