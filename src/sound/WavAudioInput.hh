// $Id$

#ifndef WAVAUDIOINPUT_HH
#define WAVAUDIOINPUT_HH

#include "AudioInputDevice.hh"
#include "SettingListener.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;

class WavAudioInput : public AudioInputDevice, private SettingListener
{
public:
	WavAudioInput(CommandController& commandController);
	virtual ~WavAudioInput();

	// AudioInputDevice
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual short readSample(const EmuTime& time);

private:
	void freeWave();
	void loadWave();
	void update(const Setting* setting);

	int length;
	byte* buffer;
	int freq;
	EmuTime reference;
	bool plugged;

	const std::auto_ptr<FilenameSetting> audioInputFilenameSetting;
};

} // namespace openmsx

#endif
