// $Id$

#ifndef MIDIINDEVICE_HH
#define MIDIINDEVICE_HH

#include "Pluggable.hh"

namespace openmsx {

class MidiInDevice : public Pluggable
{
public:
	// Pluggable (part)
	virtual const std::string& getClass() const;
	
	virtual void signal(const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
