// $Id$

#ifndef __AUDIOINPUTDEVICE_HH__
#define __AUDIOINPUTDEVICE_HH__

#include "Pluggable.hh"


class AudioInputDevice : public Pluggable
{
	public:
		/**
		 * Read wave data 
		 */
		virtual short readSample(const EmuTime &time) = 0;
		
		// Pluggable
		virtual const string &getClass() const;
};

#endif
