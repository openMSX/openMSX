// $Id$

#include "Mixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "DirectXSoundDriver.hh"
#include "SoundDevice.hh"
#include "WavWriter.hh"
#include "CliComm.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "InfoCommand.hh"
#include "GlobalSettings.hh"
#include "TclObject.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "FileOperations.hh"
#include <algorithm>
#include <cassert>

using std::remove;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

class SoundlogCommand : public SimpleCommand
{
public:
	SoundlogCommand(CommandController& commandController, Mixer& mixer);
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
private:
	std::string stopSoundLogging(const std::vector<std::string>& tokens);
	std::string startSoundLogging(const std::vector<std::string>& tokens);
	std::string toggleSoundLogging(const std::vector<std::string>& tokens);
	Mixer& mixer;
};

class SoundDeviceInfoTopic : public InfoTopic
{
public:
	SoundDeviceInfoTopic(CommandController& commandController, Mixer& mixer);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	Mixer& mixer;
};


Mixer::Mixer(Scheduler& scheduler_, CommandController& commandController_)
	: muteCount(0)
	, scheduler(scheduler_)
	, commandController(commandController_)
	, pauseSetting(commandController.getGlobalSettings().getPauseSetting())
	, soundlogCommand(new SoundlogCommand(commandController, *this))
	, soundDeviceInfo(new SoundDeviceInfoTopic(commandController, *this))
{
	driver.reset(new NullSoundDriver());
	handlingUpdate = false;
	prevLeft = outLeft = 0;
	prevRight = outRight = 0;

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
	masterVolume->attach(*this);
	frequencySetting->attach(*this);
	samplesSetting->attach(*this);
	soundDriverSetting->attach(*this);
	pauseSetting.attach(*this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) ++muteCount;
	if (pauseSetting.getValue()) ++muteCount;

	openSound();
	muteHelper();
}

Mixer::~Mixer()
{
	assert(buffers.empty());

	driver.reset();

	pauseSetting.detach(*this);
	soundDriverSetting->detach(*this);
	samplesSetting->detach(*this);
	frequencySetting->detach(*this);
	masterVolume->detach(*this);
	muteSetting->detach(*this);
}


void Mixer::reopenSound()
{
	int numBuffers = buffers.size();

	driver.reset(new NullSoundDriver());
	for (int i = 0; i < numBuffers; ++i) {
		delete[] buffers[i];
	}
	buffers.clear();

	openSound();
	for (int i = 0; i < numBuffers; ++i) {
		buffers.push_back(new int[2 * driver->getSamples()]);
	}
	muteHelper();
}

void Mixer::openSound()
{
	try {
		switch (soundDriverSetting->getValue()) {
		case SND_NULL:
			driver.reset(new NullSoundDriver());
			break;
		case SND_SDL:
			driver.reset(new SDLSoundDriver(scheduler,
				commandController.getGlobalSettings(), *this,
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#ifdef _WIN32
		case SND_DIRECTX:
			driver.reset(new DirectXSoundDriver(scheduler,
				commandController.getGlobalSettings(), *this,
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#endif
		default:
			assert(false);
		}
		handlingUpdate = true;
		//frequencySetting->setValue(driver->getFrequency());
		//samplesSetting->setValue(driver->getSamples());
		handlingUpdate = false;
	} catch (MSXException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}
}


void Mixer::registerSound(SoundDevice& device, short volume, ChannelMode mode)
{
	const string& name = device.getName();
	SoundDeviceInfo info;
	info.volumeSetting = new IntegerSetting(commandController,
		name + "_volume", "the volume of this sound chip", 75, 0, 100);

	// once we're stereo, stay stereo. Once mono, stay mono.
	// we could also choose not to offer any modeSetting in case we have
	// a stereo mode initially. You can't query the mode then, though.
	string defaultMode;
	EnumSetting<ChannelMode>::Map modeMap;
	if (mode == STEREO) {
		defaultMode = "stereo";
		modeMap[defaultMode] = STEREO;
	} else {
		defaultMode = "mono";
		modeMap[defaultMode] = MONO;
		modeMap["left"] = MONO_LEFT;
		modeMap["right"] = MONO_RIGHT;
	}
	modeMap["off"] = OFF;
	info.modeSetting = new EnumSetting<ChannelMode>(commandController,
		name + "_mode", "the channel mode of this sound chip",
		modeMap[defaultMode], modeMap, Setting::DONT_SAVE);
	info.modeSetting->setValue(mode);

	info.mode = mode;
	info.normalVolume = (volume * 100 * 100) / (75 * 75);
	info.modeSetting->attach(*this);
	info.volumeSetting->attach(*this);
	infos[&device] = info;

	lock();
	devices[mode].push_back(&device);
	device.setSampleRate(driver->getFrequency());
	device.setVolume((info.normalVolume * info.volumeSetting->getValue() *
	                   masterVolume->getValue()) / (100 * 100));
	buffers.push_back(new int[2 * driver->getSamples()]);
	muteHelper();
	unlock();
}

void Mixer::unregisterSound(SoundDevice& device)
{
	Infos::iterator it = infos.find(&device);
	if (it == infos.end()) {
		return;
	}
	lock();
	delete[] buffers.back();
	buffers.pop_back();
	ChannelMode mode = it->second.mode;
	vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), &device), dev.end());
	it->second.volumeSetting->detach(*this);
	delete it->second.volumeSetting;
	it->second.modeSetting->detach(*this);
	delete it->second.modeSetting;
	infos.erase(it);

	muteHelper();
	unlock();
}


void Mixer::updateStream(const EmuTime& time)
{
	driver->updateStream(time);
}

void Mixer::generate(short* buffer, unsigned samples,
	const EmuTime& start, const EmuDuration& sampDur)
{
	int modeOffset[NB_MODES];
	int unmuted = 0;
	for (int mode = 0; mode < NB_MODES -1; mode++) { // -1 for OFF mode
		modeOffset[mode] = unmuted;
		for (vector<SoundDevice*>::const_iterator it =
		           devices[mode].begin();
		     it != devices[mode].end(); ++it) {
			if (!(*it)->isMuted()) {
				(*it)->updateBuffer(samples, buffers[unmuted++],
				                    start, sampDur);
			}
		}
	}

	for (unsigned j = 0; j < samples; ++j) {
		int buf = 0;
		int both = 0;
		while (buf < modeOffset[MONO+1]) {
			both  += buffers[buf++][j];
		}
		int left = both;
		while (buf < modeOffset[MONO_LEFT+1]) {
			left  += buffers[buf++][j];
		}
		int right = both;
		while (buf < modeOffset[MONO_RIGHT+1]) {
			right += buffers[buf++][j];
		}
		while (buf < unmuted) {
			left  += buffers[buf]  [2*j+0];
			right += buffers[buf++][2*j+1];
		}

		// DC removal filter
		//   y(n) = x(n) - x(n-1) + R * y(n-1)
		//   R = 1 - (pi*2 * cut-off-frequency / samplerate)
		// take R = 1022/1024
		//   44100Hz --> cutt-off freq = 14Hz
		//   22050Hz                     7Hz
		outLeft   =  left -  prevLeft + ((1022 *  outLeft) >> 10);
		prevLeft  =  left;
		outRight  = right - prevRight + ((1022 * outRight) >> 10);
		prevRight = right;

		// clip
		if      (outLeft  > 32767)  outLeft  =  32767;
		else if (outLeft  < -32768) outLeft  = -32768;
		if      (outRight > 32767)  outRight =  32767;
		else if (outRight < -32768) outRight = -32768;

		buffer[2 * j + 0] = static_cast<short>(outLeft);
		buffer[2 * j + 1] = static_cast<short>(outRight);
		if (wavWriter.get()) {
			wavWriter->write16stereo(outLeft, outRight);
		}
	}
}

void Mixer::lock()
{
	driver->lock();
}

void Mixer::unlock()
{
	driver->unlock();
}

void Mixer::mute()
{
	muteCount++;
	muteHelper();
}
void Mixer::unmute()
{
	assert(muteCount);
	muteCount--;
	muteHelper();
}
void Mixer::muteHelper()
{
	if ((buffers.size() == 0) || muteCount) {
		driver->mute();
	} else {
		driver->unmute();
	}
}

// stuff for soundlogging

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
		filename = FileOperations::getNextNumberedFileName("soundlogs", "openmsx", ".wav");
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
			filename = FileOperations::getNextNumberedFileName("soundlogs", tokens[3], ".wav");
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

// end stuff for soundlogging

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
		if (handlingUpdate) return;
		handlingUpdate = true;
		reopenSound();
		handlingUpdate = false;
	} else if (&setting == frequencySetting.get()) {
		if (handlingUpdate) return;
		handlingUpdate = true;
		if (wavWriter.get()) {
			wavWriter.reset();
			commandController.getCliComm().printWarning(
				"Stopped logging sound, because of change of "
				"frequency setting");
			// the alternative: ignore the change of setting and keep logging sound
		}
		reopenSound();
		for (int mode = 0; mode < NB_MODES; ++mode) {
			for (vector<SoundDevice*>::const_iterator it =
			             devices[mode].begin();
			     it != devices[mode].end(); ++it) {
				(*it)->setSampleRate(driver->getFrequency());
			}
		}
		handlingUpdate = false;
	} else if (&setting == masterVolume.get()) {
		updateMasterVolume(masterVolume->getValue());
	} else if (dynamic_cast<const EnumSetting<ChannelMode>* >(&setting)) {
		Infos::iterator it = infos.begin();
		while (it != infos.end() && it->second.modeSetting != &setting) {
			++it;
		}
		assert(it != infos.end());
		SoundDeviceInfo &info = it->second;
		lock();
		ChannelMode oldmode = info.mode;
		info.mode = info.modeSetting->getValue();
		vector<SoundDevice*> &dev = devices[oldmode];
		dev.erase(remove(dev.begin(), dev.end(), it->first), dev.end());
		devices[info.mode].push_back(it->first);
		unlock();
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		Infos::iterator it = infos.begin();
		while (it != infos.end() && it->second.volumeSetting != &setting) {
			++it;
		}
		assert(it != infos.end());
		const SoundDeviceInfo& info = it->second;
		it->first->setVolume(
		     (masterVolume->getValue() * info.volumeSetting->getValue() *
		      info.normalVolume) / (100 * 100));
	} else {
		assert(false);
	}
}

// 0 <= mastervolume <= 100
void Mixer::updateMasterVolume(int masterVolume)
{
	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		const SoundDeviceInfo& info = it->second;
		it->first->setVolume(
		     (info.normalVolume * info.volumeSetting->getValue() *
		      masterVolume) / (100 * 100));
	}
}


// Sound device info

SoundDevice* Mixer::getSoundDevice(const string& name)
{
	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		if (it->first->getName() == name) {
			return it->first;
		}
	}
	return NULL;
}

SoundDeviceInfoTopic::SoundDeviceInfoTopic(
		CommandController& commandController, Mixer& mixer_)
	: InfoTopic(commandController, "sounddevice")
	, mixer(mixer_)
{
}

void SoundDeviceInfoTopic::execute(const vector<TclObject*>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (Mixer::Infos::const_iterator it = mixer.infos.begin();
		     it != mixer.infos.end(); ++it) {
			result.addListElement(it->first->getName());
		}
		break;
	case 3: {
		SoundDevice* device = mixer.getSoundDevice(tokens[2]->getString());
		if (!device) {
			throw CommandException("Unknown sound device");
		}
		result.setString(device->getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string SoundDeviceInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void SoundDeviceInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> devices;
		for (Mixer::Infos::const_iterator it =
		       mixer.infos.begin(); it != mixer.infos.end(); ++it) {
			devices.insert(it->first->getName());
		}
		completeString(tokens, devices);
	}
}

} // namespace openmsx
