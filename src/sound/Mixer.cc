// $Id$

#include <SDL/SDL.h>
#include <cassert>
#include "Mixer.hh"
#include "../cpu/MSXCPU.hh"
#include "RealTime.hh"
#include "SoundDevice.hh"
#include "MSXConfig.hh"


Mixer::Mixer()
{
	PRT_DEBUG("Creating a Mixer object");
	
#ifdef DEBUG
	nbClipped = 0;
#endif
	
	MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById("Mixer");
	int freq    = config->getParameterAsInt("frequency");
	int samples = config->getParameterAsInt("samples");
	SDL_AudioSpec desired;
	desired.freq     = freq;
	desired.samples  = samples;
	desired.channels = 2;			// stereo
	desired.format   = AUDIO_S16LSB;	// TODO check low|high endian
	desired.callback = audioCallbackHelper;	// must be a static method
	desired.userdata = NULL;		// not used
	if (SDL_OpenAudio(&desired, &audioSpec) < 0 )
		PRT_ERROR("Couldn't open audio : " << SDL_GetError());
	
	mixBuffer = new short[audioSpec.size / sizeof(short)];
	for (int i=0; i<NB_MODES; i++) 
	nbAllDevices = 0;
	samplesLeft = audioSpec.samples;
	offset = 0;

	cpu = MSXCPU::instance();
	realTime = RealTime::instance();
}

Mixer::~Mixer()
{
	PRT_DEBUG("Destroying a MIXER object");
	delete[] mixBuffer;
}

Mixer* Mixer::instance(void)
{
	if (oneInstance == NULL ) {
		oneInstance = new Mixer();
	}
	return oneInstance;
}
Mixer *Mixer::oneInstance = NULL;


int Mixer::registerSound(SoundDevice *device, ChannelMode mode)
{
	PRT_DEBUG("Mix: Registering sound device");
	lock();
	device->setSampleRate(audioSpec.freq);
	buffers[mode].push_back(NULL);	// make room for one more
	devices[mode].push_back(device);
	if (nbAllDevices++ == 0) SDL_PauseAudio(0);	// unpause when first dev registers
	unlock();

	return audioSpec.samples;
}

void Mixer::unregisterSound(SoundDevice *device, ChannelMode mode)
{
	PRT_DEBUG("Mix: Unregistering sound device");
	lock();
	buffers[mode].pop_back();	// remove one entry
	devices[mode].remove(device);
	if (--nbAllDevices == 0) SDL_PauseAudio(1);	// pause when last dev unregisters
	unlock();
}


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
	prevTime = cpu->getCurrentTime();
}

void Mixer::updateStream(const EmuTime &time)
{
	assert(prevTime<=time);
	float duration = realTime->getRealDuration(prevTime, time);
	PRT_DEBUG("Mix: update, duration " << duration << "s");
	assert(duration>=0);
	prevTime = time;
	lock();
	updtStrm((int)(audioSpec.freq * duration));
	unlock();
}
void Mixer::updtStrm(int samples)
{
	int nbUnmuted[NB_MODES];
	
	if (samples > samplesLeft) samples = samplesLeft;
	if (samples == 0) return;
	PRT_DEBUG("Mix: Generate " << samples << " samples");
	for (int mode=0; mode<NB_MODES; mode++) {
		int unmuted = 0;
		for (std::list<SoundDevice*>::iterator i=devices[mode].begin();
		     i != devices[mode].end(); i++) {
			int *buf = (*i)->updateBuffer(samples);
			if (buf != NULL) {
				buffers[mode][unmuted] = buf;
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

		// clip
		#ifdef DEBUG
		if (left>32767 || left<-32768 || right>32767 || right<-32768) {
			nbClipped++;
			PRT_DEBUG("Mixer: clipped "<<nbClipped);
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
	SDL_LockAudio();
}

void Mixer::unlock()
{
	SDL_UnlockAudio();
}

void Mixer::pause(bool status)
{
	if (nbAllDevices == 0)
		return;
	SDL_PauseAudio(status);
}
