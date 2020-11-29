#ifndef MIDIINDEVICE_HH
#define MIDIINDEVICE_HH

#include "Pluggable.hh"

namespace openmsx {

class MidiInDevice : public Pluggable
{
public:
	// Pluggable (part)
	[[nodiscard]] std::string_view getClass() const final;

	virtual void signal(EmuTime::param time) = 0;
};

} // namespace openmsx

#endif
