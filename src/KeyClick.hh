// $Id$

#ifndef __KEYCLICK_HH__
#define __KEYCLICK_HH__

#include "openmsx.hh"
#include "EmuTime.hh"

// forward declaration
class DACSound;

class KeyClick
{
	public:
		KeyClick(short volume, const EmuTime &time);
		virtual ~KeyClick(); 
	
		void reset(const EmuTime &time);
		void setClick(bool status, const EmuTime &time);

	private:
		DACSound* dac;
		bool status;
};
#endif
