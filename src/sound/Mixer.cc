// $Id$

#include <cassert>
#include <algorithm>
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "RealTime.hh"
#include "SoundDevice.hh"
#include "MSXConfig.hh"
#include "VolumeSetting.hh"


Mixer::Mixer()
	: muteCount(0)
{
#ifdef DEBUG_MIXER
	nbClipped = 0;
#endif
	// default values
	int freq = 22050;
	int samples = 512;
	try {
		Config *config = MSXConfig::instance()->getConfigById("Mixer");
		if (config->hasParameter("frequency")) {
			freq = config->getParameterAsInt("frequency");
		}
		if (config->hasParameter("samples")) {
			samples = config->getParameterAsInt("samples");
		}
	} catch (MSXException &e) {
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
		PRT_INFO("Couldn't open audio : " << SDL_GetError());
		init = false;
	} else {
		init = true;
		mixBuffer = new short[audioSpec.size / sizeof(short)];
		cpu = MSXCPU::instance();
		realTime = RealTime::instance();
		reInit();
	}
}

Mixer::~Mixer()
{
	if (init) {
		SDL_CloseAudio();
		delete[] mixBuffer;
	}
}

Mixer* Mixer::instance(void)
{
	static Mixer oneInstance;
	
	return &oneInstance;
}


int Mixer::registerSound(const std::string &name, SoundDevice *device,
                         short volume, ChannelMode mode)
{
	if (!init) {
		return 512;	// return a save value
	}
	SoundDeviceInfo info;
	info.volumeSetting = new VolumeSetting(name, volume, device);
	info.mode = mode;
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
	std::map<SoundDevice*, SoundDeviceInfo>::iterator it=
		infos.find(device);
	if (it == infos.end()) {
		return;
	}
	lock();
	ChannelMode mode = it->second.mode;
	std::vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), device), dev.end());
	buffers.pop_back();
	delete it->second.volumeSetting;
	
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
		std::vector<SoundDevice*>::iterator i;
		for (i = devices[mode].begin(); i != devices[mode].end(); i++) {
			int *buf = (*i)->updateBuffer(samples);
			if (buf != NULL) {
				buffers[unmuted++] = buf;
			}
		}
	}
	for (int j=0; j<samples; j++) {
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


// class MuteSetting

Mixer::MuteSetting::MuteSetting()
	: BooleanSetting("mute", "(un)mute the emulation sound", false)
{
}

bool Mixer::MuteSetting::checkUpdate(bool newValue)
{
	if (newValue != getValue()) {
		if (newValue) {
			Mixer::instance()->mute();
		} else {
			Mixer::instance()->unmute();
		}
	}
	return true;
}
