// $Id$

#include "Mixer.hh"
#include "MSXMixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "DirectXSoundDriver.hh"
#include "WavWriter.hh"
#include "CliComm.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "FileOperations.hh"
#include <algorithm>
#include <cassert>

using std::set;
using std::string;
using std::vector;

namespace openmsx {

class SoundlogCommand : public SimpleCommand
{
public:
	SoundlogCommand(CommandController& commandController, Mixer& mixer);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	string stopSoundLogging(const vector<string>& tokens);
	string startSoundLogging(const vector<string>& tokens);
	string toggleSoundLogging(const vector<string>& tokens);
	Mixer& mixer;
};


Mixer::Mixer(CommandController& commandController_)
	: muteCount(0)
	, commandController(commandController_)
	, pauseSetting(commandController.getGlobalSettings().getPauseSetting())
	, soundlogCommand(new SoundlogCommand(commandController, *this))
{
	// default values
#ifdef _WIN32
	const int defaultsamples = 2048;
#else
	const int defaultsamples = 1024;
#endif

	muteSetting.reset(new BooleanSetting(commandController,
		"mute", "(un)mute the emulation sound", false,
		Setting::DONT_SAVE));
	masterVolume.reset(new IntegerSetting(commandController,
		"master_volume", "master volume", 75, 0, 100));
	frequencySetting.reset(new IntegerSetting(commandController,
		"frequency", "mixer frequency", 44100, 11025, 48000));
	samplesSetting.reset(new IntegerSetting(commandController,
		"samples", "mixer samples", defaultsamples, 64, 8192));

	EnumSetting<SoundDriverType>::Map soundDriverMap;
	soundDriverMap["null"]    = SND_NULL;
	soundDriverMap["sdl"]     = SND_SDL;
	SoundDriverType defaultSoundDriver = SND_SDL;

#ifdef _WIN32
	soundDriverMap["directx"] = SND_DIRECTX;
	defaultSoundDriver = SND_DIRECTX;
#endif

	soundDriverSetting.reset(new EnumSetting<SoundDriverType>(
		commandController, "sound_driver",
		"select the sound output driver",
		defaultSoundDriver, soundDriverMap));

	muteSetting->attach(*this);
	frequencySetting->attach(*this);
	samplesSetting->attach(*this);
	soundDriverSetting->attach(*this);
	pauseSetting.attach(*this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) ++muteCount;
	if (pauseSetting.getValue()) ++muteCount;

	reloadDriver();
}

Mixer::~Mixer()
{
	assert(msxMixers.empty());
	driver.reset();

	pauseSetting.detach(*this);
	soundDriverSetting->detach(*this);
	samplesSetting->detach(*this);
	frequencySetting->detach(*this);
	muteSetting->detach(*this);
}

void Mixer::reloadDriver()
{
	driver.reset(new NullSoundDriver());

	try {
		switch (soundDriverSetting->getValue()) {
		case SND_NULL:
			driver.reset(new NullSoundDriver());
			break;
		case SND_SDL:
			driver.reset(new SDLSoundDriver(
				*this,
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#ifdef _WIN32
		case SND_DIRECTX:
			driver.reset(new DirectXSoundDriver(
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#endif
		default:
			assert(false);
		}
	} catch (MSXException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}

	muteHelper();
}

void Mixer::registerMixer(MSXMixer& mixer)
{
	assert(count(msxMixers.begin(), msxMixers.end(), &mixer) == 0);
	msxMixers.push_back(&mixer);

	muteHelper();
}

void Mixer::unregisterMixer(MSXMixer& mixer)
{
	assert(count(msxMixers.begin(), msxMixers.end(), &mixer) == 1);
	msxMixers.erase(find(msxMixers.begin(), msxMixers.end(), &mixer));

	muteHelper();
}


void Mixer::mute()
{
	if (muteCount++ == 0) {
		muteHelper();
	}
}

void Mixer::unmute()
{
	assert(muteCount);
	if (--muteCount == 0) {
		muteHelper();
	}
}

void Mixer::muteHelper()
{
	bool mute = muteCount || msxMixers.empty();
	bool msxMute = mute;
	for (MSXMixers::iterator it = msxMixers.begin();
	     it != msxMixers.end(); ++it) {
		unsigned samples = msxMute ? 0 : driver->getSamples();
		unsigned frequency = driver->getFrequency();
		(*it)->setMixerParams(samples, frequency);
		msxMute = true; // mute all but the first MSXMixer
	}
	
	if (mute) {
		driver->mute();
	} else {
		driver->unmute();
	}
}

IntegerSetting& Mixer::getMasterVolume() const
{
	return *masterVolume;
}

double Mixer::uploadBuffer(MSXMixer& msxMixer, short* buffer, unsigned len)
{
	// can only handle one MSXMixer ATM
	assert(!msxMixers.empty());
	assert(msxMixers.front() == &msxMixer);
	(void)msxMixer;

	if (wavWriter.get()) {
		writeWaveData(buffer, len);
	}
	return driver->uploadBuffer(buffer, len);
}

void Mixer::writeWaveData(short* buffer, unsigned samples)
{
	for (unsigned i = 0; i < samples; ++i) {
		wavWriter->write16stereo(buffer[2 * i + 0], buffer[2 * i + 1]);
	}
}

void Mixer::update(const Setting& setting)
{
	if (&setting == muteSetting.get()) {
		if (muteSetting->getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if (&setting == &pauseSetting) {
		if (pauseSetting.getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if ((&setting == samplesSetting.get()) ||
	           (&setting == soundDriverSetting.get())) {
		reloadDriver();
	} else if (&setting == frequencySetting.get()) {
		if (wavWriter.get()) {
			wavWriter.reset();
			commandController.getCliComm().printWarning(
				"Stopped logging sound, because of change of "
				"frequency setting");
		}
		reloadDriver();
	} else {
		assert(false);
	}
}


// class SoundlogCommand

SoundlogCommand::SoundlogCommand(
		CommandController& commandController, Mixer& mixer_)
	: SimpleCommand(commandController, "soundlog")
	, mixer(mixer_)
{
}

string SoundlogCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "start") {
		return startSoundLogging(tokens);
	} else if (tokens[1] == "stop") {
		return stopSoundLogging(tokens);
	} else if (tokens[1] == "toggle") {
		return toggleSoundLogging(tokens);
	}
	throw SyntaxError();
}

string SoundlogCommand::startSoundLogging(const vector<string>& tokens)
{
	string filename;
	switch (tokens.size()) {
	case 2:
		filename = FileOperations::getNextNumberedFileName(
			"soundlogs", "openmsx", ".wav");
		break;
	case 3:
		if (tokens[2] == "-prefix") {
			throw SyntaxError();
		} else {
			filename = tokens[2];
		}
		break;
	case 4:
		if (tokens[2] == "-prefix") {
			filename = FileOperations::getNextNumberedFileName(
				"soundlogs", tokens[3], ".wav");
		} else {
			throw SyntaxError();
		}
		break;
	default:
		throw SyntaxError();
	}

	if (!mixer.wavWriter.get()) {
		try {
			mixer.wavWriter.reset(new WavWriter(filename,
				2, 16, mixer.frequencySetting->getValue()));
			mixer.commandController.getCliComm().printInfo(
				"Started logging sound to " + filename);
			return filename;
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	} else {
		return "Already logging!";
	}
}

string SoundlogCommand::stopSoundLogging(const vector<string>& tokens)
{
	if (tokens.size() != 2) throw SyntaxError();
	if (mixer.wavWriter.get()) {
		mixer.wavWriter.reset();
		return "SoundLogging stopped.";
	} else {
		return "Sound logging was not enabled, are you trying to fool me?";
	}
}

string SoundlogCommand::toggleSoundLogging(const vector<string>& tokens)
{
	if (tokens.size() != 2) throw SyntaxError();
	if (!mixer.wavWriter.get()) {
		return startSoundLogging(tokens);
	} else {
		return stopSoundLogging(tokens);
	}
}

string SoundlogCommand::help(const vector<string>& /*tokens*/) const
{
	return "Controls sound logging: writing the openMSX sound to a wav file.\n"
	       "soundlog start              Log sound to file \"openmsxNNNN.wav\"\n"
	       "soundlog start <filename>   Log sound to indicated file\n"
	       "soundlog start -prefix foo  Log sound to file \"fooNNNN.wav\"\n"
	       "soundlog stop               Stop logging sound\n"
	       "soundlog toggle             Toggle sound logging state\n";
}

void SoundlogCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("start");
		cmds.insert("stop");
		cmds.insert("toggle");
		completeString(tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "start")) {
		set<string> cmds;
		cmds.insert("-prefix");
		completeString(tokens, cmds);
	}
}

} // namespace openmsx
