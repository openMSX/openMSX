// $Id: KeyClick.hh,v 

#ifndef __KEYCLICK_HH__
#define __KEYCLICK_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "emutime.hh"
#include "DACSound.hh"


class KeyClick
{
	public:
		KeyClick(); 
		virtual ~KeyClick(); 
	
		void init();
		void reset();
		void setClick(bool status, const Emutime &time);

	private:
		DACSound* dac;
		bool status;
};
#endif
