// $Id:

#include <SDL/SDL.h>
#include "Mixer.hh"
#include "MSXCPU.hh"
#include "MSXRealTime.hh"


SDL_AudioSpec Mixer::audioSpec;
int Mixer::nbAllDevices;
int Mixer::nbDevices[NB_MODES];
std::vector<SoundDevice*> Mixer::devices[NB_MODES];
std::vector<short*> Mixer::buffers[NB_MODES];
int Mixer::samplesLeft;
int Mixer::offset;
Emutime Mixer::prevTime;


Mixer::Mixer()
{
	PRT_DEBUG("Creating a Mixer object");
	
	SDL_AudioSpec desired;
	desired.freq     = 22050;	// TODO make configurable
	desired.samples  = 512;	// TODO make configurable
	desired.channels = 2;
	desired.format   = AUDIO_S16LSB;	// check MSB <-> LSB
	desired.callback = audioCallback;	// must be a static method
	desired.userdata = NULL;		// not used
	if (SDL_OpenAudio(&desired, &audioSpec) < 0 )
		PRT_ERROR("Couldn't open audio : " << SDL_GetError());
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


void Mixer::registerSound(SoundDevice *device, ChannelMode mode=MONO)
{
	PRT_DEBUG("Registering sound device");
	SDL_LockAudio();
	device->setSampleRate(audioSpec.freq);
	int bufSize = audioSpec.size / sizeof(short);
	if (mode!=STEREO) bufSize /= 2;
	short *buf = new short[bufSize];
	buffers[mode].push_back(buf);
	devices[mode].push_back(device);
	nbDevices[mode]++;
	if (++nbAllDevices == 1) SDL_PauseAudio(0);	// unpause when first device registers
	SDL_UnlockAudio();
}

//void Mixer::unregisterSound(SoundDevice *device)


void Mixer::audioCallback(void *userdata, Uint8 *strm, int len)
{
	//PRT_DEBUG("Audio callback");
	instance()->updtStrm(samplesLeft);
	short *stream = (short*)strm;
	for (int j=0; j<audioSpec.samples; j++) {
		int both = 0;
		for (int i=0; i<nbDevices[MONO]; i++)
			both  += buffers[MONO][i][j];
		int left = both;
		for (int i=0; i<nbDevices[MONO_LEFT]; i++)
			left  += buffers[MONO_LEFT][i][j];
		int right = both;
		for (int i=0; i<nbDevices[MONO_RIGHT]; i++)
			right += buffers[MONO_RIGHT][i][j];
		for (int i=0; i<nbDevices[STEREO]; i++) {
			left  += buffers[STEREO][i][2*j];
			right += buffers[STEREO][i][2*j+1];
		}
		if      (left  > 32767)  left  =  32767;
		else if (left  < -32768) left  = -32768;
		(*stream++) = (short)left;
		if      (right > 32767)  right =  32767;
		else if (right < -32768) right = -32768;
		(*stream++) = (short)right;
	}
	instance()->reInit();
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
	updtStrm(audioSpec.freq * duration);
}
void Mixer::updtStrm(int samples)
{
	if (samples == 0) return;
	if (samples > samplesLeft) samples = samplesLeft;
	PRT_DEBUG("Generate " << samples << " samples");
	for (int mode=0; mode<NB_MODES; mode++) {
		for (int i=0; i<nbDevices[mode]; i++) {
			devices[mode][i]->updateBuffer(buffers[mode][i]+offset, samples);
		}
	}
	offset += samples;
	samplesLeft -= samples;
}
