// $Id$

#ifndef __DUMMYMIDIINDEVICE_HH__
#define __DUMMYMIDIINDEVICE_HH__

#include "MidiInDevice.hh"


class DummyMidiInDevice : public MidiInDevice
{
	virtual void ready();
};

#endif

