// $Id$

#if defined(__APPLE__)

#include "MidiOutCoreMIDI.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include <mach/mach_time.h>


namespace openmsx {

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
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI endpoint (" << status << ")");
	}
	
	//struct mach_timebase_info timebaseInfo;
	//mach_timebase_info(&timebaseInfo);
}

void MidiOutCoreMIDIVirtual::unplugHelper(EmuTime::param /*time*/)
{
	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", status);
	}
	endpoint = NULL;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", status);
	}
	client = NULL;
}

const std::string& MidiOutCoreMIDIVirtual::getName() const
{
	static const std::string name("virtual-endpoint");
	return name;
}

const std::string MidiOutCoreMIDIVirtual::getDescription() const
{
	return "Outputs MIDI events via the CoreMIDI framework.";
}

void MidiOutCoreMIDIVirtual::recvByte(byte value, EmuTime::param /*time*/)
{
	// TODO: It would be better to schedule events based on EmuTime.
	MIDITimeStamp abstime = mach_absolute_time();

	byte buffer[256];
	MIDIPacketList *myPacketList = (MIDIPacketList *)buffer;
	MIDIPacket *myPacket = MIDIPacketListInit(myPacketList);
	myPacket = MIDIPacketListAdd(myPacketList, sizeof(buffer), myPacket, abstime, 1, &value);

	if (OSStatus status = MIDIReceived(endpoint, myPacketList)) {
		fprintf(stderr, "Failed to send MIDI data (%d)\n", status);
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
