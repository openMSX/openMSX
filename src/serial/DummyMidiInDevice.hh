// $Id$

#ifndef DUMMYMIDIINDEVICE_HH
#define DUMMYMIDIINDEVICE_HH

#include "MidiInDevice.hh"

namespace openmsx {

class DummyMidiInDevice : public MidiInDevice
{
public:
	virtual void signal(const EmuTime& time);
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
