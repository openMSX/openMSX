#ifndef MIDIINCOREMIDI_HH
#define MIDIINCOREMIDI_HH

#if defined(__APPLE__)

#include "MidiInDevice.hh"
#include "Thread.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include "circular_buffer.hh"
#include <CoreMIDI/MIDIServices.h>

namespace openmsx {

class EventDistributor;
class Scheduler;
class PluggingController;

/** Sends MIDI events to an existing CoreMIDI destination.
  */
class MidiInCoreMIDI : public MidiInDevice, private EventListener
{
public:
	static void registerAll(EventDistributor& eventDistributor,
                            Scheduler& scheduler, PluggingController& controller);

	/** Public for the sake of make_unique<>() - not intended for actual
	  * public use.
	  */
	explicit MidiInCoreMIDI(EventDistributor& eventDistributor_,
                            Scheduler& scheduler_, MIDIEndpointRef endpoint);
	virtual ~MidiInCoreMIDI();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;

	// MidiInDevice
	virtual void signal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);


private:
	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	static void sendPacketList(const MIDIPacketList *pktlist,
                         void *readProcRefCon, void *srcConnRefCon);
	void sendPacketList(const MIDIPacketList *pktlist, void *srcConnRefCon);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	cb_queue<byte> queue;
	Semaphore lock; // to protect queue

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
class MidiInCoreMIDIVirtual : public MidiInDevice, private EventListener
{
public:
	explicit MidiInCoreMIDIVirtual(EventDistributor& eventDistributor_,
                                   Scheduler& scheduler_);
	virtual ~MidiInCoreMIDIVirtual();

	// Pluggable
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;

	// MidiInDevice
	virtual void signal(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

	static void sendPacketList(const MIDIPacketList *pktlist,
                         void *readProcRefCon, void *srcConnRefCon);
	void sendPacketList(const MIDIPacketList *pktlist, void *srcConnRefCon);

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	cb_queue<byte> queue;
	Semaphore lock; // to protect queue

	MIDIClientRef client;
	MIDIEndpointRef endpoint;
};

} // namespace openmsx

#endif // defined(__APPLE__)
#endif // MIDIINCOREMIDI_HH
