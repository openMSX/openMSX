// $Id$

#ifndef __DUMMYMIDIOUTDEVICE_HH__
#define __DUMMYMIDIOUTDEVICE_HH__

#include "MidiOutDevice.hh"


class DummyMidiOutDevice : public MidiOutDevice
{
	public:
		// SerialDataInterface (part)
		virtual void recvByte(byte value, const EmuTime& time);
};

#endif

