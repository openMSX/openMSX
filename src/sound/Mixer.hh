// $Id$

#ifndef __MIXER_HH__
#define __MIXER_HH__

#ifdef DEBUG
//#define DEBUG_MIXER
#endif

#include <SDL/SDL.h>
#include <vector>
#include <map>
#include "EmuTime.hh"
#include "Settings.hh"

namespace openmsx {

class SoundDevice;
class MSXCPU;
class RealTime;
class VolumeSetting;


class Mixer : private SettingListener
{
	public:
		static const int MAX_VOLUME = 32767;
		enum ChannelMode {
			MONO, MONO_LEFT, MONO_RIGHT, STEREO, NB_MODES
		};

		virtual ~Mixer();
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
		int registerSound(const string &name, SoundDevice *device,
		                  short volume, ChannelMode mode);

		/**
		 * Every sounddevice must unregister before it is destructed
		 */
		void unregisterSound(SoundDevice *device);
		
		/**
		 * Use this method to force an 'early' call to all
		 * updateBuffer() methods.
		 */
		void updateStream(const EmuTime &time);

		/**
		 * This methods (un)locks the audio thread.
		 * You can use this method to delay the call to the SoundDevices
		 * updateBuffer() method. For example, this is usefull if
		 * you are updating a lot of registers and you don't want the
		 * half updated set being used to produce sound
		 */
		void lock();
		void unlock();

		/**
		 * This methods (un)mute the sound.
		 * These methods may be called multiple times, as long as
		 * you never call unmute() more than mute()
		 */
		void mute();
		void unmute();
		
	private:
		Mixer();
		void reInit();
		void updtStrm(int samples);
		static void audioCallbackHelper(void *userdata, Uint8 *stream, int len);
		void audioCallback(short* stream);
		void muteHelper(int muteCount);
		virtual void update(const SettingLeafNode *setting);

		bool init;
		int muteCount;

		struct SoundDeviceInfo {
			IntegerSetting* volumeSetting;
			EnumSetting<ChannelMode> *modeSetting;
			ChannelMode mode;
		};
		map<SoundDevice*, SoundDeviceInfo> infos;

		SDL_AudioSpec audioSpec;
		vector<SoundDevice*> devices[NB_MODES];
		vector<int*> buffers;

		short* mixBuffer;
		int samplesLeft;
		int offset;
		EmuTime prevTime;

		MSXCPU *cpu;
		RealTime *realTime;

		BooleanSetting muteSetting;

#ifdef DEBUG_MIXER
		int nbClipped;
#endif
};

} // namespace openmsx

#endif //__MIXER_HH__

