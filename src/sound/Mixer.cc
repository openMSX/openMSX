// $Id$

#include <cassert>
#include <algorithm>
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "RealTime.hh"
#include "SoundDevice.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CliCommunicator.hh"
#include "InfoCommand.hh"

namespace openmsx {

Mixer::Mixer()
	: muteCount(0),
	  muteSetting("mute", "(un)mute the emulation sound", false)
{
#ifdef DEBUG_MIXER
	nbClipped = 0;
#endif

	// default values
	int freq = 22050;
	int samples = 512;
	try {
		Config* config = MSXConfig::instance()->getConfigById("Mixer");
		freq = config->getParameterAsInt("frequency", freq);
		samples = config->getParameterAsInt("samples", samples);
	} catch (ConfigException &e) {
		// no Mixer section
	}

	SDL_AudioSpec desired;
	desired.freq     = freq;
	desired.samples  = samples;
	desired.channels = 2;			// stereo
#ifdef WORDS_BIGENDIAN
	desired.format   = AUDIO_S16MSB;
#else
	desired.format   = AUDIO_S16LSB;
#endif

	desired.callback = audioCallbackHelper;	// must be a static method
	desired.userdata = this;
	if (SDL_OpenAudio(&desired, &audioSpec) < 0) {
		CliCommunicator::instance().printWarning(
		        string("Couldn't open audio : ") + SDL_GetError());
		init = false;
	} else {
		init = true;
		mixBuffer = new short[audioSpec.size / sizeof(short)];
		cpu = MSXCPU::instance();
		realTime = RealTime::instance();
		reInit();
		muteSetting.addListener(this);
	}
	InfoCommand::instance().registerTopic("sounddevice", &soundDeviceInfo);
}

Mixer::~Mixer()
{
	// Note: Code was moved to shutDown(), read explanation there.
}

Mixer *Mixer::instance()
{
	static Mixer oneInstance;

	return &oneInstance;
}

void Mixer::shutDown()
{
	// TODO: This method is only necessary because Mixer has dependencies
	//       to the rest of openMSX (CPU, maybe more).
	//       Because of those depedencies, the destruction sequence is
	//       relevant and therefore must be made explicit.
	
	InfoCommand::instance().unregisterTopic("sounddevice", &soundDeviceInfo);
	if (init) {
		muteSetting.removeListener(this);
		SDL_CloseAudio();
		delete[] mixBuffer;
	}
}

int Mixer::registerSound(SoundDevice* device, short volume, ChannelMode mode)
{
	if (!init) {
		// sound disabled
		return 512;	// return a save value
	}
	
	const string& name = device->getName();
	SoundDeviceInfo info;
	info.volumeSetting = new IntegerSetting(name + "_volume",
			"the volume of this sound chip", volume, 0, 32767);

	map<string, ChannelMode> modeMap;
	// once we're stereo, stay stereo. Once mono, stay mono.
	// we could also choose not to offer any modeSetting in case we have
	// a stereo mode initially. You can't query the mode then, though.
	if (mode == STEREO) {
		modeMap["stereo"] = STEREO;
	} else {
		modeMap["mono"] = MONO;
		modeMap["left"] = MONO_LEFT;
		modeMap["right"] = MONO_RIGHT;
	}
	info.modeSetting = new EnumSetting<ChannelMode>(name + "_mode", "the channel mode of this sound chip", mode, modeMap);
	
	info.mode = mode;
	info.modeSetting->addListener(this);
	info.volumeSetting->addListener(this);
	infos[device] = info;

	lock();
	if (buffers.size() == 0) {
		SDL_PauseAudio(0);	// unpause when first dev registers
	}
	buffers.push_back(NULL);	// make room for one more
	devices[mode].push_back(device);
	device->setSampleRate(audioSpec.freq);
	device->setVolume(volume);
	unlock();

	return audioSpec.samples;
}

void Mixer::unregisterSound(SoundDevice *device)
{
	if (!init) {
		return;
	}
	map<SoundDevice*, SoundDeviceInfo>::iterator it=
		infos.find(device);
	if (it == infos.end()) {
		return;
	}
	lock();
	ChannelMode mode = it->second.mode;
	vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), device), dev.end());
	buffers.pop_back();
	it->second.volumeSetting->removeListener(this);
	delete it->second.volumeSetting;
	it->second.modeSetting->removeListener(this);
	delete it->second.modeSetting;
	
	if (buffers.size() == 0) {
		SDL_PauseAudio(1);	// pause when last dev unregisters
	}
	unlock();
}


void Mixer::audioCallbackHelper (void *userdata, Uint8 *strm, int len)
{
	// len ignored
	short *stream = (short*)strm;
	((Mixer*)userdata)->audioCallback(stream);
}

void Mixer::audioCallback(short* stream)
{
	updtStrm(samplesLeft);
	memcpy(stream, mixBuffer, audioSpec.size);
	reInit();
}

void Mixer::reInit()
{
	samplesLeft = audioSpec.samples;
	offset = 0;
	prevTime = cpu->getCurrentTime(); // !! can be one instruction off
}

void Mixer::updateStream(const EmuTime &time)
{
	if (!init) return;

	if (prevTime < time) {
		float duration = realTime->getRealDuration(prevTime, time);
		//PRT_DEBUG("Mix: update, duration " << duration << "s");
		assert(duration >= 0);
		prevTime = time;
		lock();
		updtStrm((int)(audioSpec.freq * duration));
		unlock();
	}
}
void Mixer::updtStrm(int samples)
{
	if (samples > samplesLeft)
		samples = samplesLeft;
	if (samples == 0)
		return;
	//PRT_DEBUG("Mix: Generate " << samples << " samples");

	int modeOffset[NB_MODES];
	int unmuted = 0;
	for (int mode = 0; mode < NB_MODES; mode++) {
		modeOffset[mode] = unmuted;
		for (vector<SoundDevice*>::const_iterator i =
		           devices[mode].begin();
		     i != devices[mode].end();
		     ++i) {
			int *buf = (*i)->updateBuffer(samples);
			if (buf != NULL) {
				buffers[unmuted++] = buf;
			}
		}
	}
	for (int j = 0; j < samples; ++j) {
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

		// clip
		#ifdef DEBUG_MIXER
		if ((left  > 32767) || (left  < -32768) ||
		    (right > 32767) || (right < -32768)) {
			nbClipped++;
			PRT_DEBUG("Mixer: clipped " << nbClipped);
		}
		#endif
		if      (left  > 32767)  left  =  32767;
		else if (left  < -32768) left  = -32768;
		if      (right > 32767)  right =  32767;
		else if (right < -32768) right = -32768;

		mixBuffer[offset++] = (short)left;
		mixBuffer[offset++] = (short)right;
	}
	samplesLeft -= samples;
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
	muteHelper(++muteCount);
}
void Mixer::unmute()
{
	assert(muteCount);
	muteHelper(--muteCount);
}
void Mixer::muteHelper(int muteCount)
{
	if (!init) return;

	if (buffers.size() == 0)
		return;
	SDL_PauseAudio(muteCount);
}


void Mixer::update(const SettingLeafNode *setting)
{
	if (setting == &muteSetting) {
		if (muteSetting.getValue()) {
			mute();
		} else {
			unmute();
		}
	} else {
		const EnumSetting<ChannelMode>* s = dynamic_cast <const EnumSetting<ChannelMode>* >(setting);
		if (s!=NULL) {
			map<SoundDevice*, SoundDeviceInfo>::iterator it = infos.begin();
			while (it != infos.end() && it->second.modeSetting != setting) ++it;
			assert (it!=infos.end());
			// it->first is the SoundDevice we need
			SoundDeviceInfo &info = it->second;
			lock();
			ChannelMode oldmode = info.mode;
			info.mode = info.modeSetting->getValue();
			vector<SoundDevice*> &dev = devices[oldmode];
			dev.erase(remove(dev.begin(), dev.end(), it->first), dev.end());
			devices[info.mode].push_back(it->first);
			unlock();
		} else {
			const IntegerSetting* t = dynamic_cast <const IntegerSetting*>(setting);
			if (t != NULL) {
				map<SoundDevice*, SoundDeviceInfo>::iterator it = infos.begin();
				while (it != infos.end() && it->second.volumeSetting != setting) ++it;
				assert (it!=infos.end());
				// it->first is the SoundDevice we need
				it->first->setVolume(it->second.volumeSetting->getValue()); 
			} else {
				assert(false);
			}
		}
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

string Mixer::SoundDeviceInfoTopic::execute(const vector<string> &tokens) const
	throw(CommandException)
{
	string result;
	Mixer* mixer = Mixer::instance();
	switch (tokens.size()) {
	case 2:
		for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
		       mixer->infos.begin(); it != mixer->infos.end(); ++it) {
			result += it->first->getName() + '\n';
		}
		break;
	case 3: {
		SoundDevice* device = mixer->getSoundDevice(tokens[2]);
		if (!device) {
			throw CommandException("Unknown sound device");
		}
		result = device->getDescription();
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
	return result;
}

string Mixer::SoundDeviceInfoTopic::help(const vector<string> &tokens) const
	throw()
{
	return "Shows a list of available sound devices.\n";
}

void Mixer::SoundDeviceInfoTopic::tabCompletion(vector<string>& tokens) const
	throw()
{
	if (tokens.size() == 3) {
		Mixer* mixer = Mixer::instance();
		set<string> devices;
		for (map<SoundDevice*, SoundDeviceInfo>::const_iterator it =
		       mixer->infos.begin(); it != mixer->infos.end(); ++it) {
			devices.insert(it->first->getName());
		}
		CommandController::completeString(tokens, devices);
	}
}

} // namespace openmsx
