// $Id$

#ifndef __MIXER_HH__
#define __MIXER_HH__

#include <SDL/SDL.h>
#include <list>
#include <vector>
#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"

class Mixer
{
	public:
		static const int MAX_VOLUME = 32767;
		enum ChannelMode { MONO, MONO_LEFT, MONO_RIGHT, STEREO };
		static const int NB_MODES = STEREO+1;

		~Mixer();
		static Mixer *instance();
		
		/**
		 * Use this method to register a given sounddevice.
		 *
		 * While registering, the device its setSampleRate() method is
		 * called (see SoundDevice for more info).
		 * After registration the device its updateBuffer() method is
		 * 'regularly' called. It asks the device to fill a buffer with
		 * a certain number of samples. (see SoundDevice for more info)
		 * The maximum number of samples asked for is returned by this
		 * method.
		 */
		int registerSound(SoundDevice *device, ChannelMode mode=MONO);
	
		/**
		 * Every sounddevice must unregister before it is destructed
		 */
		void unregisterSound(SoundDevice *device, ChannelMode mode=MONO);
		
		/**
		 * Use this method to force an 'early' call to all
		 * updateBuffer() methods. 
		 */
		void updateStream(const EmuTime &time);

		/**
		 * This method pauses the sound
		 */
		void pause(bool status);
		
	private:
		Mixer();
		void reInit();
		void updtStrm(int samples);
		static void audioCallbackHelper(void *userdata, Uint8 *stream, int len);
		void audioCallback(short* stream);
		
		static Mixer *oneInstance;

		SDL_AudioSpec audioSpec;
		int nbAllDevices;
		int nbUnmuted[NB_MODES];
		std::list<SoundDevice*> devices[NB_MODES];
		std::vector<int*> buffers[NB_MODES];
		
		short* mixBuffer;
		int samplesLeft;
		int offset;
		EmuTime prevTime;
};

#endif //__MIXER_HH__

