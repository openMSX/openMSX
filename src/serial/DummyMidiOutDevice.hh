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
	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time) throw();
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif

