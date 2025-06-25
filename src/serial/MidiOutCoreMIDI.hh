#ifndef MIDIOUTCOREMIDI_HH
#define MIDIOUTCOREMIDI_HH

#if defined(__APPLE__)

#include "MidiOutDevice.hh"
#include <CoreMIDI/MIDIServices.h>

#include <vector>

namespace openmsx {

class PluggingController;

/** Puts MIDI messages into a MIDIPacketList.
  */
class MidiOutMessageBuffer : public MidiOutDevice
{
protected:
	virtual OSStatus sendPacketList(MIDIPacketList *myPacketList) = 0;

private:
	void recvMessage(
			const std::vector<uint8_t>& message, EmuTime time) override;
};

/** Sends MIDI events to an existing CoreMIDI destination.
  */
class MidiOutCoreMIDI final : public MidiOutMessageBuffer
{
public:
	static void registerAll(PluggingController& controller);

	/** Public for the sake of make_unique<>() - not intended for actual
	  * public use.
	  */
	explicit MidiOutCoreMIDI(MIDIEndpointRef endpoint);

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiOutMessageBuffer
	OSStatus sendPacketList(MIDIPacketList *myPacketList) override;

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
class MidiOutCoreMIDIVirtual final : public MidiOutMessageBuffer
{
public:
	explicit MidiOutCoreMIDIVirtual();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiOutMessageBuffer
	OSStatus sendPacketList(MIDIPacketList *myPacketList) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MIDIClientRef client;
	MIDIEndpointRef endpoint;
};

} // namespace openmsx

#endif // defined(__APPLE__)
#endif // MIDIOUTCOREMIDI_HH
