// $Id: KeyClick.hh,v 

#ifndef __KEYCLICK_HH__
#define __KEYCLICK_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include "DACSound.hh"


class KeyClick
{
	public:
		KeyClick(); 
		virtual ~KeyClick(); 
	
		void reset();
		void setClick(bool status, const EmuTime &time);

	private:
		DACSound* dac;
		bool status;
};
#endif
