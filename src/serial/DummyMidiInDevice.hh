// $Id$

#ifndef __DUMMYMIDIINDEVICE_HH__
#define __DUMMYMIDIINDEVICE_HH__

#include "MidiInDevice.hh"


class DummyMidiInDevice : public MidiInDevice
{
	public:
		virtual void ready();
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime& time);
};

#endif

