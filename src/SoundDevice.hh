// $Id$

#ifndef __SOUNDDEVICE_HH__
#define __SOUNDDEVICE_HH__

#include "openmsx.hh"

class SoundDevice
{
	public:
		/**
		 *
		 */
		virtual void init();

		/**
		 *
		 */
		virtual void reset();

		/**
		 * Set the relative volume for this sound device, this
		 * can be used to make a MSX-MUSIC sound louder than a
		 * MSX-AUDIO
		 * 
		 *    0 <= newVolume <= 32767
		 */
		virtual void setVolume (int newVolume);

		/**
		 *
		 */
		virtual void setSampleRate (int newSampleRate);

		/**
		 * TODO update sound buffers
		 */
		virtual void updateBuffer (short *buffer, int length);
};
#endif
