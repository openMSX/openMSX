// $Id$

#ifndef __DUMMYMIDIOUTDEVICE_HH__
#define __DUMMYMIDIOUTDEVICE_HH__

#include "MidiOutDevice.hh"


namespace openmsx {

class DummyMidiOutDevice : public MidiOutDevice
{
	public:
		// SerialDataInterface (part)
		virtual void recvByte(byte value, const EmuTime& time);
		virtual void plug(Connector* connector, const EmuTime& time) throw();
		virtual void unplug(const EmuTime& time);
};

} // namespace openmsx

#endif

