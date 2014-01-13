#if defined(__APPLE__)

#include "MidiOutCoreMIDI.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "serialize.hh"
#include "memory.hh"
#include "openmsx.hh"

#include <mach/mach_time.h>
#include <cassert>


namespace openmsx {

// MidiOutMessageBuffer ======================================================

static constexpr uint8_t MIDI_MSG_SYSEX     = 0xF0;
static constexpr uint8_t MIDI_MSG_SYSEX_END = 0xF7;

/** Returns the size in bytes of a message that starts with the given status.
  */
static size_t midiMessageLength(uint8_t status)
{
	if (status < 0x80) {
		assert(false);
		return 0;
	} else if (status < 0xC0) {
		return 3;
	} else if (status < 0xE0) {
		return 2;
	} else if (status < 0xF0) {
		return 3;
	} else {
		switch (status) {
		case MIDI_MSG_SYSEX:
			// Limit to force sending large SysEx messages in chunks.
			// The limit for the amount of data in one MIDIPacket in CoreMIDI,
			// so our chunks could be rejected by CoreMIDI if we set this limit
			// larger than 256.
			return 256;
		case MIDI_MSG_SYSEX_END:
			assert(false);
			return 0;
		case 0xF1:
		case 0xF3:
			return 2;
		case 0xF2:
			return 3;
		case 0xF4:
		case 0xF5:
		case 0xF9:
		case 0xFD:
			PRT_DEBUG("Data size unknown for MIDI status"
			          "0x" << std::hex << int(status) << std::dec);
			return 1;
		default:
			return 1;
		}
	}
}

MidiOutMessageBuffer::MidiOutMessageBuffer()
	: isSysEx(false)
{
}

void MidiOutMessageBuffer::clearBuffer()
{
	message.clear();
	isSysEx = false;
}

// Note: Nothing in this method is specific to CoreMIDI; it could be moved to
//       generic MIDI code if any other platform also needs message rebuilding.
void MidiOutMessageBuffer::recvByte(byte value, EmuTime::param time)
{
	if (value & 0x80) { // status byte
		if (value == MIDI_MSG_SYSEX_END) {
			if (isSysEx) {
				message.push_back(value);
				messageComplete(time);
			} else {
				PRT_DEBUG("Ignoring SysEx end without start");
			}
			message.clear();
			isSysEx = false;
		} else {
			// Replace any message in progress.
			message = { value };
			isSysEx = value == MIDI_MSG_SYSEX;
		}
	} else { // data byte
		if (message.empty() && !isSysEx) {
			PRT_DEBUG("Ignoring MIDI data without preceding status: "
			          "0x" << std::hex << int(value) << std::dec);
		} else {
			uint8_t status = isSysEx ? MIDI_MSG_SYSEX : message[0];
			size_t len = midiMessageLength(status);
			message.push_back(value);
			if (message.size() >= len) {
				messageComplete(time);
				if (isSysEx || len <= 1) {
					message.clear();
				} else {
					// Keep last status, to support running status.
					message.resize(1);
				}
			}
		}
	}
}

void MidiOutMessageBuffer::messageComplete(EmuTime::param /*time*/)
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
		fprintf(stderr, "Failed to send MIDI data (%d)\n", (int)status);
	} else {
		//fprintf(stderr, "MIDI send OK: %02X\n", value);
	}
}


// MidiOutCoreMIDI ===========================================================

void MidiOutCoreMIDI::registerAll(PluggingController& controller)
{
	ItemCount numberOfEndpoints = MIDIGetNumberOfDestinations();
	for (ItemCount i = 0; i < numberOfEndpoints; i++) {
		MIDIEndpointRef endpoint = MIDIGetDestination(i);
		if (endpoint) {
			controller.registerPluggable(
				make_unique<MidiOutCoreMIDI>(endpoint));
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
	if (OSStatus status = MIDIClientCreate(CFSTR("openMSX"), nullptr, nullptr, &client)) {
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI client (" << status << ")");
	}
	// Create output port.
	if (OSStatus status = MIDIOutputPortCreate(client, CFSTR("Output"), &port)) {
		MIDIClientDispose(client);
		client = 0;
		throw PlugException(StringOp::Builder() <<
			"Failed to create MIDI port (" << status << ")");
	}
}

void MidiOutCoreMIDI::unplugHelper(EmuTime::param /*time*/)
{
	clearBuffer();

	// Dispose of the client; this automatically disposes of the port as well.
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	port = 0;
	client = 0;
}

const std::string& MidiOutCoreMIDI::getName() const
{
	return name;
}

string_ref MidiOutCoreMIDI::getDescription() const
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
	clearBuffer();

	if (OSStatus status = MIDIEndpointDispose(endpoint)) {
		fprintf(stderr, "Failed to dispose of MIDI port (%d)\n", (int)status);
	}
	endpoint = 0;
	if (OSStatus status = MIDIClientDispose(client)) {
		fprintf(stderr, "Failed to dispose of MIDI client (%d)\n", (int)status);
	}
	client = 0;
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
