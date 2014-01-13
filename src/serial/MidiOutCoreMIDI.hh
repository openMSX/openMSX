#ifndef MIDIOUTCOREMIDI_HH
#define MIDIOUTCOREMIDI_HH

#if defined(__APPLE__)

#include "MidiOutDevice.hh"
#include <CoreMIDI/MIDIServices.h>

#include <vector>

namespace openmsx {

class PluggingController;

/** Combines MIDI bytes into full MIDI messages.
  * CoreMIDI expects full messages in packet lists.
  */
class MidiOutMessageBuffer : public MidiOutDevice
{
public:
	// SerialDataInterface (part)
	virtual void recvByte(byte value, EmuTime::param time) override;

protected:
	explicit MidiOutMessageBuffer();
	void clearBuffer();

	virtual OSStatus sendPacketList(MIDIPacketList *myPacketList) = 0;

private:
	void messageComplete(EmuTime::param /*time*/);

	std::vector<uint8_t> message;
	bool isSysEx;
};

/** Sends MIDI events to an existing CoreMIDI destination.
  */
class MidiOutCoreMIDI : public MidiOutMessageBuffer
{
public:
	static void registerAll(PluggingController& controller);

	/** Public for the sake of make_unique<>() - not intended for actual
	  * public use.
	  */
	explicit MidiOutCoreMIDI(MIDIEndpointRef endpoint);

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time) override;
	virtual void unplugHelper(EmuTime::param time) override;
	virtual const std::string& getName() const override;
	virtual string_ref getDescription() const override;

	// MidiOutMessageBuffer
	virtual OSStatus sendPacketList(MIDIPacketList *myPacketList) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
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
class MidiOutCoreMIDIVirtual : public MidiOutMessageBuffer
{
public:
	explicit MidiOutCoreMIDIVirtual();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time) override;
	virtual void unplugHelper(EmuTime::param time) override;
	virtual const std::string& getName() const override;
	virtual string_ref getDescription() const override;

	// MidiOutMessageBuffer
	virtual OSStatus sendPacketList(MIDIPacketList *myPacketList) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MIDIClientRef client;
	MIDIEndpointRef endpoint;
};

} // namespace openmsx

#endif // defined(__APPLE__)
#endif // MIDIOUTCOREMIDI_HH
