// $Id$

#ifndef __DUMMYMIDIINDEVICE_HH__
#define __DUMMYMIDIINDEVICE_HH__

#include "MidiInDevice.hh"


namespace openmsx {

class DummyMidiInDevice : public MidiInDevice
{
	public:
		virtual void signal(const EmuTime& time);
		virtual const string& getDescription() const;
		virtual void plug(Connector* connector, const EmuTime& time) throw();
		virtual void unplug(const EmuTime& time);
};

} // namespace openmsx

#endif

