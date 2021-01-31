#if defined(__APPLE__)

#include "MidiOutCoreMIDI.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include "openmsx.hh"
#include "StringOp.hh"

#include <mach/mach_time.h>
#include <cassert>
#include <memory>


namespace openmsx {

// MidiOutMessageBuffer ======================================================

void MidiOutMessageBuffer::recvMessage(
		const std::vector<uint8_t>& message, EmuTime::param /*time*/)
{
	// TODO: It would be better to schedule events based on EmuTime.
	MIDITimeStamp abstime = mach_absolute_time();

	MIDIPacketList packetList;
	MIDIPacket *curPacket = MIDIPacketListInit(&packetList);
	curPacket = MIDIPacketListAdd(&packetList, sizeof(packetList),
			curPacket, abstime, message.size(), message.data());
	if (!curPacket) {
		fprintf(stderr, "Failed to package MIDI data\n");
	} else if (OSStatus status = sendPacketList(&packetList)) {
		fprintf(stderr, "Failed to send MIDI data (%d)\n", int(status));
	} else {
		//fprintf(stderr, "MIDI send OK: %02X\n", value);
	}
}


// MidiOutCoreMIDI ===========================================================

void MidiOutCoreMIDI::registerAll(PluggingController& controller)
{
	for (auto i : xrange(MIDIGetNumberOfDestinations())) {
		if (MIDIEndpointRef endpoint = MIDIGetDestination(i)) {
			controller.registerPluggable(
				std::make_unique<MidiOutCoreMIDI>(endpoint));
		}
	}
}

MidiOutCoreMIDI::MidiOutCoreMIDI(MIDIEndpointRef endpoint_)
	: endpoint(endpoint_)
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
		name = strCat(StringOp::fromCFString(midiDeviceName), " OUT");
		CFRelease(midiDeviceName);
	}
}

void MidiOutCoreMIDI::plugHelper(Connector& /*connector*/,
                                 EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"), nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create output port.
	if (OSStatus status = MIDIOutputPortCreate(client, CFSTR("Output"), &port)) {
		MIDIClientDispose(client);
		client = 0;
		throw PlugException("Failed to create MIDI port (", status, ')');
	}
}

void MidiOutCoreMIDI::unplugHelper(EmuTime::param /*time*/)
{
	clearBuffer();

	// Dispose of the client; this automatically disposes of the port as well.
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", int(status));
	}
	port = 0;
	client = 0;
}

std::string_view MidiOutCoreMIDI::getName() const
{
	return name;
}

std::string_view MidiOutCoreMIDI::getDescription() const
{
	return "Sends MIDI events to an existing CoreMIDI destination.";
}

OSStatus MidiOutCoreMIDI::sendPacketList(MIDIPacketList *myPacketList)
{
	return MIDISend(port, endpoint, myPacketList);
}

template<typename Archive>
void MidiOutCoreMIDI::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutCoreMIDI);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutCoreMIDI, "MidiOutCoreMIDI");


// MidiOutCoreMIDIVirtual ====================================================

MidiOutCoreMIDIVirtual:: MidiOutCoreMIDIVirtual()
	: client(0)
	, endpoint(0)
{
}

void MidiOutCoreMIDIVirtual::plugHelper(Connector& /*connector*/,
                                        EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"), nullptr, nullptr, &client)) {
		throw PlugException("Failed to create MIDI client (", status, ')');
	}
	// Create endpoint.
	if (OSStatus status = MIDISourceCreate(client, CFSTR("openMSX"), &endpoint)) {
		MIDIClientDispose(client);
		throw PlugException("Failed to create MIDI endpoint (", status, ')');
	}
}

void MidiOutCoreMIDIVirtual::unplugHelper(EmuTime::param /*time*/)
{
	clearBuffer();

	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", int(status));
	}
	endpoint = 0;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", int(status));
	}
	client = 0;
}

std::string_view MidiOutCoreMIDIVirtual::getName() const
{
	return "Virtual OUT";
}

std::string_view MidiOutCoreMIDIVirtual::getDescription() const
{
	return "Sends MIDI events from a newly created CoreMIDI virtual source.";
}

OSStatus MidiOutCoreMIDIVirtual::sendPacketList(MIDIPacketList *myPacketList)
{
	return MIDIReceived(endpoint, myPacketList);
}

template<typename Archive>
void MidiOutCoreMIDIVirtual::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutCoreMIDIVirtual);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutCoreMIDIVirtual, "MidiOutCoreMIDIVirtual");

} // namespace openmsx

#endif // defined(__APPLE__)
