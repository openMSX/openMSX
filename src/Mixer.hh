// $Id:

#ifndef __MIXER_HH__
#define __MIXER_HH__

#include <SDL/SDL.h>
#include <vector>
#include "openmsx.hh"
#include "SoundDevice.hh"
#include "emutime.hh"

class Mixer
{
	// TODO remove static declarations
	public:
		enum ChannelMode { MONO, MONO_LEFT, MONO_RIGHT, STEREO };
		static const int NB_MODES = STEREO+1;

		~Mixer();
		static Mixer *instance();
		
		void registerSound(SoundDevice *device, ChannelMode mode=MONO);
		//void unregisterSound(SoundDevice *device);
		void updateStream(const Emutime &time);
		
	private:
		Mixer();
		void reInit();
		void updtStrm(int samples);
		
		static Mixer *oneInstance;

		static void audioCallback(void *userdata, Uint8 *stream, int len);

		static SDL_AudioSpec audioSpec;
		static int nbAllDevices;
		static int nbDevices[NB_MODES];
		static std::vector<SoundDevice*> devices[NB_MODES];
		static std::vector<short*> buffers[NB_MODES];

		static int samplesLeft;
		static int offset;
		static Emutime prevTime;
};

#endif //__MIXER_HH__

