// $Id:

#include <SDL/SDL.h>
#include "Mixer.hh"

SDL_AudioSpec Mixer::audioSpec;
int Mixer::nbDevices;
std::vector<SoundDevice*> Mixer::devices;
std::vector<short*> Mixer::buffers;


Mixer::Mixer()
{
	PRT_DEBUG("Creating a Mixer object");
	
	SDL_AudioSpec desired;
	// TODO make these values configurable
	desired.freq     = 22050;
	desired.format   = AUDIO_S16LSB;
	desired.channels = 1;
	desired.samples  = 8192;
	desired.callback = audioCallback;
	desired.userdata = NULL;
	if (SDL_OpenAudio(&desired, &audioSpec) < 0 )
		PRT_ERROR("Couldn't open audio : " << SDL_GetError());
	nbDevices = 0;
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


void Mixer::registerSound(SoundDevice *device)
{
	PRT_DEBUG("Registering sound device");
	SDL_LockAudio();
	short *buf = new short[audioSpec.size/2];
	buffers.push_back(buf);
	devices.push_back(device);
	if (++nbDevices == 1) SDL_PauseAudio(0);	// unpause when first device registers
	SDL_UnlockAudio();
}

//void Mixer::unregisterSound(SoundDevice *device)


void Mixer::audioCallback(void *userdata, Uint8 *strm, int len)
{
	int length = len/2;
	short *stream = (short*)strm;
	for (int i=0; i<nbDevices; i++) {
		devices[i]->updateBuffer(buffers[i], length);
	}
	for (int j=0; j<length; j++) {
		int temp = 0;
		for (int i=0; i<nbDevices; i++)
			temp += buffers[i][j];
		if      (temp > 32767)  temp =  32767;
		else if (temp < -32768) temp = -32768;
		(*stream++) = temp;
	}
}

