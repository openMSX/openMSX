// $Id$

#include "Mixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "SoundDevice.hh"
#include "CliComm.hh"
#include "InfoCommand.hh"
#include "GlobalSettings.hh"
#include "CommandArgument.hh"
#include "CommandLineParser.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include <algorithm>
#include <cassert>

#include <dirent.h>
#include "ReadDir.hh"
#include "FileOperations.hh"
#include "FileException.hh"

using std::map;
using std::remove;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

Mixer::Mixer()
	: muteCount(0)
	, output(CliComm::instance())
	, infoCommand(InfoCommand::instance())
	, pauseSetting(GlobalSettings::instance().getPauseSetting())
	, soundlogCommand(*this)
	, soundDeviceInfo(*this)
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

	muteSetting.reset(new BooleanSetting(
		"mute", "(un)mute the emulation sound", false,
		Setting::DONT_SAVE));
	masterVolume.reset(new IntegerSetting(
		"master_volume", "master volume", 75, 0, 100));
	frequencySetting.reset(new IntegerSetting("frequency",
		"mixer frequency",
		44100, 11025, 48000));
	samplesSetting.reset(new IntegerSetting("samples",
		"mixer samples",
		defaultsamples, 64, 8192));

	EnumSetting<SoundDriverType>::Map soundDriverMap;
	soundDriverMap["null"] = SND_NULL;
	soundDriverMap["sdl"]  = SND_SDL;
	SoundDriverType defaultSoundDriver =
		CommandLineParser::instance().wantSound() ? SND_SDL : SND_NULL;
	soundDriverSetting.reset(new EnumSetting<SoundDriverType>(
		"sound_driver", "select the sound output driver",
		defaultSoundDriver, soundDriverMap));

	infoCommand.registerTopic("sounddevice", &soundDeviceInfo);
	muteSetting->addListener(this);
	masterVolume->addListener(this);
	frequencySetting->addListener(this);
	samplesSetting->addListener(this);
	soundDriverSetting->addListener(this);
	pauseSetting.addListener(this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) ++muteCount;
	if (pauseSetting.getValue()) ++muteCount;

	openSound();
	muteHelper();

	wavfp = NULL;
	CommandController::instance().registerCommand(&soundlogCommand, "soundlog");
}

Mixer::~Mixer()
{
	assert(buffers.empty());
	
	CommandController::instance().unregisterCommand(&soundlogCommand, "soundlog");

	endSoundLogging();
	driver.reset();
	
	pauseSetting.removeListener(this);
	soundDriverSetting->removeListener(this);
	samplesSetting->removeListener(this);
	frequencySetting->removeListener(this);
	masterVolume->removeListener(this);
	muteSetting->removeListener(this);
	infoCommand.unregisterTopic("sounddevice", &soundDeviceInfo);
}

Mixer& Mixer::instance()
{
	static Mixer oneInstance;
	return oneInstance;
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
			driver.reset(new SDLSoundDriver(*this,
			                    frequencySetting->getValue(),
			                    samplesSetting->getValue()));
			break;
		default:
			assert(false);
		}
		handlingUpdate = true;
		frequencySetting->setValue(driver->getFrequency());
		samplesSetting->setValue(driver->getSamples());
		handlingUpdate = false;
	} catch (MSXException& e) {
		output.printWarning(e.getMessage());
	}
}


void Mixer::registerSound(SoundDevice& device, short volume, ChannelMode mode)
{
	const string& name = device.getName();
	SoundDeviceInfo info;
	info.volumeSetting = new IntegerSetting(
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
	info.modeSetting = new EnumSetting<ChannelMode>(
		name + "_mode", "the channel mode of this sound chip",
		modeMap[defaultMode], modeMap, Setting::DONT_SAVE);
	info.modeSetting->setValue(mode);
	
	info.mode = mode;
	info.normalVolume = (volume * 100 * 100) / (75 * 75);
	info.modeSetting->addListener(this);
	info.volumeSetting->addListener(this);
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
	map<SoundDevice*, SoundDeviceInfo>::iterator it=
		infos.find(&device);
	if (it == infos.end()) {
		return;
	}
	lock();
	delete[] buffers.back();
	buffers.pop_back();
	ChannelMode mode = it->second.mode;
	vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), &device), dev.end());
	it->second.volumeSetting->removeListener(this);
	delete it->second.volumeSetting;
	it->second.modeSetting->removeListener(this);
	delete it->second.modeSetting;
	infos.erase(it);

	muteHelper();
	unlock();
}


void Mixer::updateStream(const EmuTime& time)
{
	driver->updateStream(time);
}

void Mixer::generate(short* buffer, unsigned samples)
{
	int modeOffset[NB_MODES];
	int unmuted = 0;
	for (int mode = 0; mode < NB_MODES -1; mode++) { // -1 for OFF mode
		modeOffset[mode] = unmuted;
		for (vector<SoundDevice*>::const_iterator it =
		           devices[mode].begin();
		     it != devices[mode].end(); ++it) {
			if (!(*it)->isMuted()) {
				(*it)->updateBuffer(samples, buffers[unmuted++]);
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
		if (wavfp) {
			// TODO check write error
			fwrite(&buffer[2 * j], 4, 1, wavfp);
			nofWavBytes += 4;
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

// TODO: generalize this with ScreenShotSaver.cc
static int getNum(dirent* d)
{
	string name(d->d_name);
	if ((name.length() != 15) ||
	    (name.substr(0, 7) != "openmsx") ||
	    (name.substr(11, 4) != ".wav")) {
		return 0;
	}
	string num(name.substr(7, 4));
	char* endpos;
	unsigned long n = strtoul(num.c_str(), &endpos, 10);
	if (*endpos != '\0') {
		return 0;
	}
	return n;
}

// TODO: generalize this with ScreenShotSaver.cc
string Mixer::SoundlogCommand::getFileName()
{
	int max_num = 0;
	
	string dirName = FileOperations::getUserOpenMSXDir() + "/soundlogs";
	try {
		FileOperations::mkdirp(dirName);
	} catch (FileException& e) {
		// ignore
	}

	ReadDir dir(dirName.c_str());
	while (dirent* d = dir.getEntry()) {
		max_num = std::max(max_num, getNum(d));
	}

	std::ostringstream os;
	os << dirName << "/openmsx";
	os.width(4);
	os.fill('0');
	os << (max_num + 1) << ".wav";
	return os.str();
}

Mixer::SoundlogCommand::SoundlogCommand(Mixer& outer_)
	: outer(outer_)
{
}

string Mixer::SoundlogCommand::execute(const vector<string>& tokens)
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

string Mixer::SoundlogCommand::startSoundLogging(const vector<string>& tokens)
{
	string filename;
	switch (tokens.size()) {
	case 2:
		filename = getFileName();
		break;
	case 3:
		filename = tokens[2];
		break;
	default:
		throw SyntaxError();
	}
	
	if (!outer.wavfp) {
		outer.startSoundLogging(filename);
		CliComm::instance().printInfo("Started logging sound to " + filename);
		return filename;
	} else {
		return "Already logging!";
	}
}

string Mixer::SoundlogCommand::stopSoundLogging(const vector<string>& tokens)
{
	if (tokens.size() != 2) throw SyntaxError();
	if (outer.wavfp) {
		outer.endSoundLogging();
		return "SoundLogging stopped.";
	} else {
		return "Sound logging was not enabled, are you trying to fool me?";
	}
}
	
string Mixer::SoundlogCommand::toggleSoundLogging(const vector<string>& tokens)
{
	if (tokens.size() != 2) throw SyntaxError();
	if (!outer.wavfp) {
		return startSoundLogging(tokens);
	} else {
		return stopSoundLogging(tokens);
	}
}

string Mixer::SoundlogCommand::help(const vector<string>& /*tokens*/) const
{
	return "Controls sound logging: writing the openMSX sound to a wav file.\n"
	       "soundlog start             Log sound to file \"openmsxNNNN.wav\"\n"
	       "soundlog start <filename>  Log sound to indicated file\n"
	       "soundlog stop              Stop logging sound\n"
	       "soundlog toggle            Toggle sound logging state\n";
}

void Mixer::SoundlogCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("start");
		cmds.insert("stop");
		cmds.insert("toggle");
		CommandController::completeString(tokens, cmds);
	}
}

void Mixer::startSoundLogging(const string& filename)
{
	/* TODO on the soundlogger:
	 * - take care of endianness (this probably won't work on PPC now)
	 * - properly unite the getFileName stuff with ScreenShotSaver
	 * - error handling, fwrite or fopen may fail miserably
	 */

	assert(!wavfp);
	
	wavfp = fopen(filename.c_str(), "wb");
	if (!wavfp) {
		// TODO
	}
	nofWavBytes = 0;

	// write wav header
	char header[44] = {
		'R', 'I', 'F', 'F', //
		0, 0, 0, 0,         // total size (filled in later)
		'W', 'A', 'V', 'E', //
		'f', 'm', 't', ' ', //
		16, 0, 0, 0,        // size of fmt block
		1, 0,               // format tag = 1
		2, 0,               // nb of channels = 2
		0, 0, 0, 0,         // samples per second (filled in)
		0, 0, 0, 0,         // avg bytes per second (filled in)
		4, 0,               // block align (2 * bits per samp / 16)
		16, 0,              // bits per sample
		'd', 'a', 't', 'a', //
		0, 0, 0, 0,         // size of data block (filled in later)
	};
	const int channels = 2;
	const int bitsPerSample = 16;
	uint32 samplesPerSecond = frequencySetting->getValue();
	*reinterpret_cast<uint32*>(header + 24) = samplesPerSecond;
	*reinterpret_cast<uint32*>(header + 28) = (channels * samplesPerSecond *
	                                           bitsPerSample) / 8;
	fwrite(header, sizeof(header), 1, wavfp);
}

void Mixer::endSoundLogging()
{
	if (wavfp) {
		// TODO error handling
		uint32 totalsize = nofWavBytes + 44;
		fseek(wavfp, 4, SEEK_SET);
		fwrite(&totalsize, 4, 1, wavfp);
		fseek(wavfp, 40, SEEK_SET);
		fwrite(&nofWavBytes, 4, 1, wavfp);

		fclose(wavfp);
		wavfp = NULL;
	}
}

// end stuff for soundlogging

void Mixer::update(const Setting* setting)
{
	if (setting == muteSetting.get()) {
		if (muteSetting->getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if (setting == &pauseSetting) {
		if (pauseSetting.getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if ((setting == samplesSetting.get()) ||
	           (setting == soundDriverSetting.get())) {
		if (handlingUpdate) return;
		handlingUpdate = true;
		reopenSound();
		handlingUpdate = false;
	} else if (setting == frequencySetting.get()) {
		if (handlingUpdate) return;
		handlingUpdate = true;
		if (wavfp != 0) {
			endSoundLogging();
			CliComm::instance().printWarning("Stopped logging sound, because of change of frequency setting");
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
	} else if (setting == masterVolume.get()) {
		updateMasterVolume(masterVolume->getValue());
	} else if (dynamic_cast<const EnumSetting<ChannelMode>* >(setting)) {
		map<SoundDevice*, SoundDeviceInfo>::iterator it = infos.begin();
		while (it != infos.end() && it->second.modeSetting != setting) {
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
	} else if (dynamic_cast<const IntegerSetting*>(setting)) {
		map<SoundDevice*, SoundDeviceInfo>::iterator it = infos.begin();
		while (it != infos.end() && it->second.volumeSetting != setting) {
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
	for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it = infos.begin();
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
	for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
	       infos.begin(); it != infos.end(); ++it) {
		if (it->first->getName() == name) {
			return it->first;
		}
	}
	return NULL;
}

Mixer::SoundDeviceInfoTopic::SoundDeviceInfoTopic(Mixer& outer_)
	: outer(outer_)
{
}

void Mixer::SoundDeviceInfoTopic::execute(const vector<CommandArgument>& tokens,
	CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2:
		for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
		       outer.infos.begin(); it != outer.infos.end(); ++it) {
			result.addListElement(it->first->getName());
		}
		break;
	case 3: {
		SoundDevice* device = outer.getSoundDevice(tokens[2].getString());
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

string Mixer::SoundDeviceInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void Mixer::SoundDeviceInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> devices;
		for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
		       outer.infos.begin(); it != outer.infos.end(); ++it) {
			devices.insert(it->first->getName());
		}
		CommandController::completeString(tokens, devices);
	}
}

} // namespace openmsx
