// $Id:

#ifndef __MIXER_HH__
#define __MIXER_HH__

#include <SDL/SDL.h>
#include <vector>
#include "openmsx.hh"
#include "SoundDevice.hh"

class Mixer
{
	public:
		~Mixer();
		static Mixer *instance();
		
		void registerSound(SoundDevice *device);
		//void unregisterSound(SoundDevice *device);
	private:
		Mixer();
		static Mixer *oneInstance;

		static void audioCallback(void *userdata, Uint8 *stream, int len);

		static SDL_AudioSpec audioSpec;
		static int nbDevices;
		static std::vector<SoundDevice*> devices;
		static std::vector<short*> buffers;
};

#endif //__MIXER_HH__

