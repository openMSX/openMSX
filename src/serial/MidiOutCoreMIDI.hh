// $Id$

#ifndef MIDIOUTCOREMIDI_HH
#define MIDIOUTCOREMIDI_HH

#if defined(__APPLE__)

#include "MidiOutDevice.hh"
#include <CoreMIDI/MIDIServices.h>

namespace openmsx {

class PluggingController;

/** Sends MIDI events to an existing CoreMIDI destination.
  */
class MidiOutCoreMIDI : public MidiOutDevice
{
public:
	static void registerAll(PluggingController& controller);

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
	explicit MidiOutCoreMIDI(MIDIEndpointRef endpoint);

	MIDIClientRef client;
	MIDIPortRef port;
	MIDIEndpointRef endpoint;

	std::string name;
};

/** Sends MIDI events from a newly created CoreMIDI virtual source.
  * This class acts as a MIDI input, unlike the other class that sends events
  * to a MIDI output. It is similar to using an IAC bus, but doesn't require
  * prior configuration to work.
  */
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
