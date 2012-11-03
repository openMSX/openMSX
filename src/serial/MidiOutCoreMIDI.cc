// $Id$

#if defined(__APPLE__)

#include "MidiOutCoreMIDI.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include <mach/mach_time.h>


namespace openmsx {

// MidiOutCoreMIDI ===========================================================

void MidiOutCoreMIDI::registerAll(PluggingController& controller)
{
	ItemCount numberOfEndpoints = MIDIGetNumberOfDestinations();
	for (ItemCount i = 0; i < numberOfEndpoints; i++) {
		MIDIEndpointRef endpoint = MIDIGetDestination(i);
		if (endpoint) {
			controller.registerPluggable(std::unique_ptr<Pluggable>(
				new MidiOutCoreMIDI(endpoint)));
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
		name = StringOp::fromCFString(midiDeviceName);
		CFRelease(midiDeviceName);
	}
}

void MidiOutCoreMIDI::plugHelper(Connector& /*connector*/,
                               EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"), NULL, NULL, &client)) {
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI client (" << status << ")");
	}
	// Create output port.
	if (OSStatus status = MIDIOutputPortCreate(client, CFSTR("Output"), &port)) {
		MIDIClientDispose(client);
		client = NULL;
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI port (" << status << ")");
	}
}

void MidiOutCoreMIDI::unplugHelper(EmuTime::param /*time*/)
{
	// Dispose of the client; this automatically disposes of the port as well.
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	port = NULL;
	client = NULL;
}

const std::string& MidiOutCoreMIDI::getName() const
{
	return name;
}

string_ref MidiOutCoreMIDI::getDescription() const
{
	return "Sends MIDI events to an existing CoreMIDI destination.";
}

void MidiOutCoreMIDI::recvByte(byte value, EmuTime::param /*time*/)
{
	// TODO: It would be better to schedule events based on EmuTime.
	MIDITimeStamp abstime = 0;

	Byte buffer[256];
	MIDIPacketList *myPacketList = (MIDIPacketList *)buffer;
	MIDIPacket *myPacket = MIDIPacketListInit(myPacketList);
	myPacket = MIDIPacketListAdd(
			myPacketList, sizeof(buffer), myPacket, abstime, 1, &value);

	if (OSStatus status = MIDISend(port, endpoint, myPacketList)) {
		fprintf(stderr, "Failed to send MIDI data (%d)\n", (int)status);
	} else {
		//fprintf(stderr, "MIDI send OK: %02X\n", value);
	}
}

template<typename Archive>
void MidiOutCoreMIDI::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutCoreMIDI);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutCoreMIDI, "MidiOutCoreMIDI");

// MidiOutCoreMIDIVirtual ====================================================

MidiOutCoreMIDIVirtual:: MidiOutCoreMIDIVirtual()
	: client(NULL)
	, endpoint(NULL)
{
}

void MidiOutCoreMIDIVirtual::plugHelper(Connector& /*connector*/,
                               EmuTime::param /*time*/)
{
	// Create client.
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"), NULL, NULL, &client)) {
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI client (" << status << ")");
	}
	// Create endpoint.
	if (OSStatus status = MIDISourceCreate(client, CFSTR("openMSX"), &endpoint)) {
		MIDIClientDispose(client);
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI endpoint (" << status << ")");
	}

	//struct mach_timebase_info timebaseInfo;
	//mach_timebase_info(&timebaseInfo);
}

void MidiOutCoreMIDIVirtual::unplugHelper(EmuTime::param /*time*/)
{
	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", (int)status);
	}
	endpoint = NULL;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	client = NULL;
}

const std::string& MidiOutCoreMIDIVirtual::getName() const
{
	static const std::string name("Virtual");
	return name;
}

string_ref MidiOutCoreMIDIVirtual::getDescription() const
{
	return "Sends MIDI events from a newly created CoreMIDI virtual source.";
}

void MidiOutCoreMIDIVirtual::recvByte(byte value, EmuTime::param /*time*/)
{
	// TODO: It would be better to schedule events based on EmuTime.
	MIDITimeStamp abstime = mach_absolute_time();

	Byte buffer[256];
	MIDIPacketList *myPacketList = (MIDIPacketList *)buffer;
	MIDIPacket *myPacket = MIDIPacketListInit(myPacketList);
	myPacket = MIDIPacketListAdd(
			myPacketList, sizeof(buffer), myPacket, abstime, 1, &value);

	if (OSStatus status = MIDIReceived(endpoint, myPacketList)) {
		fprintf(stderr, "Failed to send MIDI data (%d)\n", (int)status);
	} else {
		//fprintf(stderr, "MIDI send OK: %02X\n", value);
	}
}

template<typename Archive>
void MidiOutCoreMIDIVirtual::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutCoreMIDIVirtual);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutCoreMIDIVirtual, "MidiOutCoreMIDIVirtual");

} // namespace openmsx

#endif // defined(__APPLE__)
