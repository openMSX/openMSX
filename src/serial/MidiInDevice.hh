// $Id$

#ifndef __MIDIINDEVICE_HH__
#define __MIDIINDEVICE_HH__

#include "Pluggable.hh"


namespace openmsx {

class MidiInDevice : public Pluggable
{
	public:
		// Pluggable (part)
		virtual const string& getClass() const;
		
		virtual void signal(const EmuTime& time) = 0;
};

} // namespace openmsx

#endif

