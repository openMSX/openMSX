// $Id: KeyClick.hh,v 

#ifndef __KEYCLICK_HH__
#define __KEYCLICK_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "emutime.hh"


class KeyClick : public SoundDevice
{
	public:
		KeyClick(); 
		virtual ~KeyClick(); 
	
		void init();
		void reset();
		void setClick(bool status, const Emutime &time);

		//SoundDevice
		void setInternalVolume(short newVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);

	private:
		int bufSize;
		int* buffer;
};
#endif
