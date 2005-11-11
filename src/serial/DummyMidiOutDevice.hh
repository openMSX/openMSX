// $Id$

#ifndef DUMMYMIDIOUTDEVICE_HH
#define DUMMYMIDIOUTDEVICE_HH

#include "MidiOutDevice.hh"

namespace openmsx {

class DummyMidiOutDevice : public MidiOutDevice
{
public:
	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
