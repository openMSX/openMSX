// $Id:

#include <SDL/SDL.h>
#include <cassert>
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "MSXRealTime.hh"


Mixer::Mixer()
{
	PRT_DEBUG("Creating a Mixer object");
	
	SDL_AudioSpec desired;
	desired.freq     = 22050;		// TODO make configurable
	desired.samples  = 512;			// TODO make configurable
	desired.channels = 2;			// stereo
	desired.format   = AUDIO_S16LSB;	// TODO check low|high endian
	desired.callback = audioCallbackHelper;	// must be a static method
	desired.userdata = NULL;		// not used
	if (SDL_OpenAudio(&desired, &audioSpec) < 0 )
		PRT_ERROR("Couldn't open audio : " << SDL_GetError());
	
	mixBuffer = new short[audioSpec.size / sizeof(short)];
	for (int i=0; i<NB_MODES; i++) 
		nbDevices[i] = 0;
	nbAllDevices = 0;
	reInit();
}

Mixer::~Mixer()
{
	PRT_DEBUG("Destroying a MIXER object");
}

Mixer* Mixer::instance(void)
{
	if (oneInstance == NULL ) {
		oneInstance = new Mixer();
	}
	return oneInstance;
}
Mixer *Mixer::oneInstance = NULL;


int Mixer::registerSound(SoundDevice *device, ChannelMode mode=MONO)
{
	PRT_DEBUG("Registering sound device");
	SDL_LockAudio();
	device->setSampleRate(audioSpec.freq);
	buffers[mode].push_back(NULL);	// make room for one more
	devices[mode].push_back(device);
	nbDevices[mode]++;
	if (++nbAllDevices == 1) SDL_PauseAudio(0);	// unpause when first device registers
	SDL_UnlockAudio();

	return audioSpec.samples;
}

//void Mixer::unregisterSound(SoundDevice *device)


void Mixer::audioCallbackHelper (void *userdata, Uint8 *strm, int len)
{
	// userdata and len are ignored
	short *stream = (short*)strm;
	oneInstance->audioCallback(stream);
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
	prevTime = MSXCPU::instance()->getCurrentTime();
}

void Mixer::updateStream(const Emutime &time)
{
	assert(prevTime<=time);
	float duration = MSXRealTime::instance()->getRealDuration(prevTime, time);
	prevTime = time;
	SDL_LockAudio();
	updtStrm(audioSpec.freq * duration);
	SDL_UnlockAudio();
}
void Mixer::updtStrm(int samples)
{
	if (samples == 0) return;
	if (samples > samplesLeft) samples = samplesLeft;
	PRT_DEBUG("Generate " << samples << " samples");
	for (int mode=0; mode<NB_MODES; mode++) {
		int unmuted = 0;
		for (int i=0; i<nbDevices[mode]; i++) {
			if (!devices[mode][i]->isInternalMuted()) {
				buffers[mode][unmuted] = devices[mode][i]->updateBuffer(samples);
				unmuted++;
			}
		}
		nbUnmuted[mode] = unmuted;
	}
	for (int j=0; j<samples; j++) {
		int both = 0;
		for (int i=0; i<nbUnmuted[MONO]; i++)
			both  += buffers[MONO][i][j];
		int left = both;
		for (int i=0; i<nbUnmuted[MONO_LEFT]; i++)
			left  += buffers[MONO_LEFT][i][j];
		int right = both;
		for (int i=0; i<nbUnmuted[MONO_RIGHT]; i++)
			right += buffers[MONO_RIGHT][i][j];
		for (int i=0; i<nbUnmuted[STEREO]; i++) {
			left  += buffers[STEREO][i][2*j];
			right += buffers[STEREO][i][2*j+1];
		}
		if      (left  > 32767)  left  =  32767;
		else if (left  < -32768) left  = -32768;
		mixBuffer[offset++] = (short)left;
		if      (right > 32767)  right =  32767;
		else if (right < -32768) right = -32768;
		mixBuffer[offset++] = (short)right;
	}
	samplesLeft -= samples;
}
