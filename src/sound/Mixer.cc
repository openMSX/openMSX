// $Id$

#include "Mixer.hh"
#include "MSXCPU.hh"
#include "RealTime.hh"
#include "SoundDevice.hh"
#include "SettingsConfig.hh"
#include "CliCommOutput.hh"
#include "InfoCommand.hh"
#include "GlobalSettings.hh"
#include "CommandArgument.hh"
#include "CommandLineParser.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "Scheduler.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

using std::map;
using std::remove;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

Mixer::Mixer()
	: muteCount(0)
	, cpu(MSXCPU::instance())
	, realTime(RealTime::instance())
	, settingsConfig(SettingsConfig::instance())
	, output(CliCommOutput::instance())
	, infoCommand(InfoCommand::instance())
	, pauseSetting(GlobalSettings::instance().getPauseSetting())
	, speedSetting(GlobalSettings::instance().getSpeedSetting())
	, throttleSetting(GlobalSettings::instance().getThrottleSetting())
	, soundDeviceInfo(*this)
{
	init = false;
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

	infoCommand.registerTopic("sounddevice", &soundDeviceInfo);
	muteSetting->addListener(this);
	masterVolume->addListener(this);
	frequencySetting->addListener(this);
	samplesSetting->addListener(this);
	pauseSetting.addListener(this);
	speedSetting.addListener(this);
	throttleSetting.addListener(this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) muteCount++;
	if (pauseSetting.getValue()) muteCount++;

	openSound();
	muteHelper();
}

Mixer::~Mixer()
{
	closeSound();
	
	throttleSetting.removeListener(this);
	speedSetting.removeListener(this);
	pauseSetting.removeListener(this);
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

	closeSound();
	for (int i = 0; i < numBuffers; ++i) {
		delete[] buffers[i];
	}
	buffers.clear();

	openSound();
	for (int i = 0; i < numBuffers; ++i) {
		buffers.push_back(new int[2 * audioSpec.samples]);
	}
	muteHelper();
}

static int roundUpPower2(int a)
{
	int res = 1;
	while (a > res) {
		res <<= 1;
	}
	return res;
}

void Mixer::openSound()
{
	if (!CommandLineParser::instance().wantSound()) {
		return;
	}
	
	SDL_AudioSpec desired;
	desired.freq     = frequencySetting->getValue();
	desired.samples  = roundUpPower2(samplesSetting->getValue());
	desired.channels = 2;			// stereo
	desired.format   = OPENMSX_BIGENDIAN ? AUDIO_S16MSB : AUDIO_S16LSB;
	desired.callback = audioCallbackHelper;	// must be a static method
	desired.userdata = this;
	
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
		if (SDL_OpenAudio(&desired, &audioSpec) == 0) {
			frequencySetting->setValue(audioSpec.freq);
			samplesSetting->setValue(audioSpec.samples);
			bufferSize = 4 * audioSpec.size / (2 * sizeof(short));
			mixBuffer = new short[2 * bufferSize];
			memset(mixBuffer, 0, bufferSize * 2 * sizeof(short));
			readPtr = writePtr = 0;
			reInit();
			prevTime = Scheduler::instance().getCurrentTime();
			EmuDuration interval2 = interval1 * audioSpec.samples;
			Scheduler::instance().setSyncPoint(prevTime + interval2, this);
			init = true;
		} else {
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
	}
	if (!init) {
		output.printWarning(
			string("Couldn't open audio: ") + SDL_GetError());
	}
}

void Mixer::closeSound()
{
	if (init) {
		Scheduler::instance().removeSyncPoint(this);
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		delete[] mixBuffer;
		init = false;
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
	device.setSampleRate(init ? audioSpec.freq : 44100);
	device.setVolume((info.normalVolume * info.volumeSetting->getValue() *
	                   masterVolume->getValue()) / (100 * 100));
	if (init) {
		buffers.push_back(new int[2 * audioSpec.samples]);
	}
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
	if (init) {
		delete[] buffers.back();
		buffers.pop_back();
	}
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


void Mixer::audioCallbackHelper (void* userdata, Uint8* strm, int len)
{
	short *stream = (short*)strm;
	((Mixer*)userdata)->audioCallback(stream, len / (2 * sizeof(short)));
}

void Mixer::audioCallback(short* stream, unsigned len)
{
	unsigned available = (readPtr <= writePtr)
	                   ? writePtr - readPtr
	                   : writePtr - readPtr + bufferSize;
	
	if (available < ( 3 * bufferSize / 8)) {
		int missing = len - available;
		if (missing <= 0) {
			// 1/4 full, speed up a little
			if (interval1.length() > 100) { // may not become 0
				interval1 /= 1.005;
			}
			//cout << "Mixer: low      " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		} else {
			// buffer underrun
			if (interval1.length() > 100) { // may not become 0
				interval1 /= 1.01;
			}
			//cout << "Mixer: underrun " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
			updtStrm2(missing);
		}
		EmuDuration minDuration = (intervalAverage * 255) / 256;
		if (interval1 < minDuration) {
			interval1 = minDuration;
			//cout << "Mixer: clipped  " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		}
	}
	if ((readPtr + len) < bufferSize) {
		memcpy(stream, &mixBuffer[2 * readPtr], len * 2 * sizeof(short));
		readPtr += len;
	} else {
		unsigned len1 = bufferSize - readPtr;
		memcpy(stream, &mixBuffer[2 * readPtr], len1 * 2 * sizeof(short));
		unsigned len2 = len - len1;
		memcpy(&stream[2 * len1], mixBuffer, len2 * 2 * sizeof(short));
		readPtr = len2;
	}
	intervalAverage = (intervalAverage * 63 + interval1) / 64;
}


void Mixer::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (!muteCount) {
		// TODO not schedule at all if muted
		updateStream(time);
	}
	EmuDuration interval2 = interval1 * audioSpec.samples;
	Scheduler::instance().setSyncPoint(time + interval2, this);
}

const string& Mixer::schedName() const
{
	static const string NAME = "mixer";
	return NAME;
}


void Mixer::updateStream(const EmuTime& time)
{
	if (!init) return;

	assert(prevTime <= time);
	EmuDuration duration = time - prevTime;
	unsigned samples = duration / interval1;
	if (samples == 0) {
		return;
	}
	prevTime += interval1 * samples;
	
	lock();
	updtStrm(samples);
	unlock();
	
}
void Mixer::updtStrm(unsigned samples)
{
	if (samples > audioSpec.samples) {
		samples = audioSpec.samples;
	}
	
	unsigned available = (readPtr <= writePtr)
	                   ? writePtr - readPtr
	                   : writePtr - readPtr + bufferSize;
	available += samples;
	if (available > (7 * bufferSize / 8)) {
		int overflow = available - (bufferSize - 1);
		if (overflow <= 0) {
			// 7/8 full slow down a bit
			interval1 *= 1.005;
			//cout << "Mixer: high     " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		} else {
			// buffer overrun
			interval1 *= 1.01;
			//cout << "Mixer: overrun  " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
			samples -= overflow;
		}
		EmuDuration maxDuration = (intervalAverage * 257) / 256;
		if (interval1 > maxDuration) {
			interval1 = maxDuration;
			//cout << "Mixer: clipped  " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		}
	}
	updtStrm2(samples);
}

void Mixer::updtStrm2(unsigned samples)
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

		mixBuffer[2 * writePtr + 0] = (short)outLeft;
		mixBuffer[2 * writePtr + 1] = (short)outRight;
		if (++writePtr == bufferSize) {
			writePtr = 0;
		}
	}
}

void Mixer::lock()
{
	if (!init) return;
	SDL_LockAudio();
}

void Mixer::unlock()
{
	if (!init) return;
	SDL_UnlockAudio();
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
	if (!init) return;
	SDL_PauseAudio(buffers.size() == 0 ? 1 : muteCount);
	reInit();
}

void Mixer::reInit()
{
	double percent = speedSetting.getValue();
	interval1 = EmuDuration(percent / (audioSpec.freq * 100));
	intervalAverage = EmuDuration(percent / (audioSpec.freq * 100));
}

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
	} else if (setting == samplesSetting.get()) {
		if (handlingUpdate) return;
		handlingUpdate = true;
		reopenSound();
		reInit();
		handlingUpdate = false;
	} else if (setting == frequencySetting.get()) {
		if (handlingUpdate) return;
		handlingUpdate = true;
		reopenSound();
		reInit();
		for (int mode = 0; mode < NB_MODES; ++mode) {
			for (vector<SoundDevice*>::const_iterator it =
			             devices[mode].begin();
			     it != devices[mode].end(); ++it) {
				(*it)->setSampleRate(init ? audioSpec.freq : 44100);
			}
		}
		handlingUpdate = false;
	} else if (setting == &speedSetting) {
		reInit();
	} else if (setting == &throttleSetting) {
		if (throttleSetting.getValue()) {
			reInit();
		}
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

Mixer::SoundDeviceInfoTopic::SoundDeviceInfoTopic(Mixer& parent_)
	: parent(parent_)
{
}

void Mixer::SoundDeviceInfoTopic::execute(const vector<CommandArgument>& tokens,
	CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2:
		for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
		       parent.infos.begin(); it != parent.infos.end(); ++it) {
			result.addListElement(it->first->getName());
		}
		break;
	case 3: {
		SoundDevice* device = parent.getSoundDevice(tokens[2].getString());
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
		       parent.infos.begin(); it != parent.infos.end(); ++it) {
			devices.insert(it->first->getName());
		}
		CommandController::completeString(tokens, devices);
	}
}

} // namespace openmsx
