// $Id$

#ifndef __MIDIINDEVICE_HH__
#define __MIDIINDEVICE_HH__

#include "Pluggable.hh"


class MidiInDevice : public Pluggable
{
	public:
		// Pluggable (part)
		virtual const string& getClass() const;
		
		virtual void ready() = 0;
};

#endif

