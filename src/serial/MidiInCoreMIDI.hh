#ifndef MIDIINCOREMIDI_HH
#define MIDIINCOREMIDI_HH

#ifdef __APPLE__

#include "MidiInDevice.hh"
#include "EventListener.hh"
#include "serialize_meta.hh"
#include "circular_buffer.hh"
#include <CoreMIDI/MIDIServices.h>

#include <cstdint>
#include <mutex>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

/** Sends MIDI events to an existing CoreMIDI destination.
  */
class MidiInCoreMIDI final : public MidiInDevice, private EventListener
{
public:
	static void registerAll(EventDistributor& eventDistributor,
                            Scheduler& scheduler, PluggingController& controller);

	/** Public for the sake of make_unique<>() - not intended for actual
	  * public use.
	  */
	explicit MidiInCoreMIDI(EventDistributor& eventDistributor_,
                            Scheduler& scheduler_, MIDIEndpointRef endpoint);
	~MidiInCoreMIDI();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);


private:
	// EventListener
	bool signalEvent(const Event& event) override;

	static void sendPacketList(const MIDIPacketList *pktList,
	                           void *readProcRefCon, void *srcConnRefCon);
	void sendPacketList(const MIDIPacketList *pktList, void *srcConnRefCon);

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue

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
class MidiInCoreMIDIVirtual final : public MidiInDevice, private EventListener
{
public:
	explicit MidiInCoreMIDIVirtual(EventDistributor& eventDistributor_,
                                   Scheduler& scheduler_);
	~MidiInCoreMIDIVirtual();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	zstring_view getName() const override;
	zstring_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// EventListener
	bool signalEvent(const Event& event) override;

	static void sendPacketList(const MIDIPacketList *pktList,
	                           void *readProcRefCon, void *srcConnRefCon);
	void sendPacketList(const MIDIPacketList *pktList, void *srcConnRefCon);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue

	MIDIClientRef client;
	MIDIEndpointRef endpoint;
};

} // namespace openmsx

#endif // defined(__APPLE__)
#endif // MIDIINCOREMIDI_HH
