// $Id$

#ifndef MIDIOUTCOREMIDI_HH
#define MIDIOUTCOREMIDI_HH

#if defined(__APPLE__)

#include "MidiOutDevice.hh"
#include <CoreMIDI/MIDIServices.h>

namespace openmsx {

class MidiOutCoreMIDIVirtual : public MidiOutDevice
{
public:
	explicit MidiOutCoreMIDIVirtual();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual const std::string getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MIDIClientRef client;
	MIDIEndpointRef endpoint;
};

} // namespace openmsx

#endif // defined(__APPLE__)
#endif // MIDIOUTCOREMIDI_HH
