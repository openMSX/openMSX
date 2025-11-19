#ifdef __APPLE__

#include "MidiInCoreMIDI.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "serialize.hh"
#include "StringOp.hh"
#include "xrange.hh"
#include <mach/mach_time.h>
#include <memory>


namespace openmsx {

// MidiInCoreMIDI ===========================================================

void MidiInCoreMIDI::registerAll(EventDistributor& eventDistributor,
                                 Scheduler& scheduler,
                                 PluggingController& controller)
{
	for (auto i : xrange(MIDIGetNumberOfSources())) {
		if (MIDIEndpointRef endpoint = MIDIGetSource(i)) {
			controller.registerPluggable(std::make_unique<MidiInCoreMIDI>(
					eventDistributor, scheduler, endpoint));
		}
	}
}

MidiInCoreMIDI::MidiInCoreMIDI(EventDistributor& eventDistributor_,
                               Scheduler& scheduler_, MIDIEndpointRef endpoint_)
	: eventDistributor(eventDistributor_)
	, scheduler(scheduler_)
	, endpoint(endpoint_)
{
	// Get a user-presentable name for the endpoint.
	CFStringRef midiDeviceName;
	OSStatus status = MIDIObjectGetStringProperty(
						endpoint, kMIDIPropertyDisplayName, &midiDeviceName);
	if (status) {
		status = MIDIObjectGetStringProperty(
						endpoint, kMIDIPropertyName, &midiDeviceName);
	}
	if (status) {
		name = "Nameless endpoint";
	} else {
		name = strCat(StringOp::fromCFString(midiDeviceName), " IN");
		CFRelease(midiDeviceName);
	}

	eventDistributor.registerEventListener(
			EventType::MIDI_IN_COREMIDI, *this);
}

MidiInCoreMIDI::~MidiInCoreMIDI()
{
	eventDistributor.unregisterEventListener(
			EventType::MIDI_IN_COREMIDI, *this);
}

void MidiInCoreMIDI::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(
			CFSTR("openMSX"), nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create input port.
	if (OSStatus status = MIDIInputPortCreate(
			client, CFSTR("Input"), sendPacketList, this, &port)) {
		MIDIClientDispose(client);
		client = 0;
		throw PlugException("Failed to create MIDI port (", status, ')');
	}

	MIDIPortConnectSource(port, endpoint, nullptr);
}

void MidiInCoreMIDI::unplugHelper(EmuTime /*time*/)
{
	// Dispose of the client; this automatically disposes of the port as well.
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", int(status));
	}
	port = 0;
	client = 0;
}

zstring_view MidiInCoreMIDI::getName() const
{
	return name;
}

zstring_view MidiInCoreMIDI::getDescription() const
{
	return "Receives MIDI events from an existing CoreMIDI source.";
}

void MidiInCoreMIDI::sendPacketList(const MIDIPacketList *packetList,
                                    void *readProcRefCon, void *srcConnRefCon)
{
	static_cast<MidiInCoreMIDI*>(readProcRefCon)
			->sendPacketList(packetList, srcConnRefCon);
}

void MidiInCoreMIDI::sendPacketList(const MIDIPacketList *packetList,
                                    void * /*srcConnRefCon*/) {
	{
		std::scoped_lock lock(mutex);
		const MIDIPacket* packet = &packetList->packet[0];
		repeat(packetList->numPackets, [&] {
			for (auto j : xrange(packet->length)) {
				queue.push_back(packet->data[j]);
			}
			packet = MIDIPacketNext(packet);
		});
	}
	eventDistributor.distributeEvent(MidiInCoreMidiEvent());
}

// MidiInDevice
void MidiInCoreMIDI::signal(EmuTime time)
{
	auto connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		std::scoped_lock lock(mutex);
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}

	uint8_t data;
	{
		std::scoped_lock lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	connector->recvByte(data, time);
}

// EventListener
bool MidiInCoreMIDI::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return false;
}

template<typename Archive>
void MidiInCoreMIDI::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiInCoreMIDI);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInCoreMIDI, "MidiInCoreMIDI");


// MidiInCoreMIDIVirtual ====================================================

MidiInCoreMIDIVirtual::MidiInCoreMIDIVirtual(EventDistributor& eventDistributor_,
                                             Scheduler& scheduler_)
	: eventDistributor(eventDistributor_)
	, scheduler(scheduler_)
	, client(0)
	, endpoint(0)
{
	eventDistributor.registerEventListener(
			EventType::MIDI_IN_COREMIDI_VIRTUAL, *this);
}

MidiInCoreMIDIVirtual::~MidiInCoreMIDIVirtual()
{
	eventDistributor.unregisterEventListener(
			EventType::MIDI_IN_COREMIDI_VIRTUAL, *this);
}

void MidiInCoreMIDIVirtual::plugHelper(Connector& /*connector*/,
                                       EmuTime /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"),
	                                       nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create endpoint.
	if (OSStatus status = MIDIDestinationCreate(client, CFSTR("openMSX"),
	                                            sendPacketList, this,
	                                            &endpoint)) {
		MIDIClientDispose(client);
		throw PlugException("Failed to create MIDI endpoint (", status, ')');
	}
}

void MidiInCoreMIDIVirtual::unplugHelper(EmuTime /*time*/)
{
	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", int(status));
	}
	endpoint = 0;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", int(status));
	}
	client = 0;
}

zstring_view MidiInCoreMIDIVirtual::getName() const
{
	return "Virtual IN";
}

zstring_view MidiInCoreMIDIVirtual::getDescription() const
{
	return "Sends MIDI events from a newly created CoreMIDI virtual source.";
}

void MidiInCoreMIDIVirtual::sendPacketList(const MIDIPacketList *packetList,
                                           void *readProcRefCon,
                                           void *srcConnRefCon)
{
	static_cast<MidiInCoreMIDIVirtual*>(readProcRefCon)
			->sendPacketList(packetList, srcConnRefCon);
}

void MidiInCoreMIDIVirtual::sendPacketList(const MIDIPacketList *packetList,
                                           void * /*srcConnRefCon*/)
{
	{
		std::scoped_lock lock(mutex);
		const MIDIPacket* packet = &packetList->packet[0];
		repeat(packetList->numPackets, [&] {
			for (auto j : xrange(packet->length)) {
				queue.push_back(packet->data[j]);
			}
			packet = MIDIPacketNext(packet);
		});
	}
	eventDistributor.distributeEvent(MidiInCoreMidiVirtualEvent());
}

// MidiInDevice
void MidiInCoreMIDIVirtual::signal(EmuTime time)
{
	auto connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		std::scoped_lock lock(mutex);
		queue.clear();
		return;
	}
	if (!connector->ready()) {
		return;
	}

	uint8_t data;
	{
		std::scoped_lock lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	connector->recvByte(data, time);
}

// EventListener
bool MidiInCoreMIDIVirtual::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return false;
}

template<typename Archive>
void MidiInCoreMIDIVirtual::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiInCoreMIDIVirtual);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInCoreMIDIVirtual, "MidiInCoreMIDIVirtual");

} // namespace openmsx

#endif // defined(__APPLE__)
