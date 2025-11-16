#include "MidiOutDevice.hh"

#include "unreachable.hh"

namespace openmsx {

static constexpr uint8_t MIDI_MSG_SYSEX     = 0xF0;
static constexpr uint8_t MIDI_MSG_SYSEX_END = 0xF7;
static constexpr uint8_t MIDI_MSG_RESET     = 0xFF;

/** Returns the size in bytes of a message that starts with the given status.
  */
[[nodiscard]] static constexpr size_t midiMessageLength(uint8_t status)
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
			return MidiOutDevice::MAX_MESSAGE_SIZE;
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
			// Data size unknown
			return 1;
		default:
			return 1;
		}
	}
}

zstring_view MidiOutDevice::getClass() const
{
	return "midi out";
}

void MidiOutDevice::clearBuffer()
{
	buffer.clear();
	isSysEx = false;
}

void MidiOutDevice::recvByte(uint8_t value, EmuTime time)
{
	if (value & 0x80) { // status byte
		if (value == MIDI_MSG_SYSEX_END) {
			if (isSysEx) {
				buffer.push_back(value);
				recvMessage(buffer, time);
			} else {
				// Ignoring SysEx end without start
			}
			buffer.clear();
			isSysEx = false;
		} else if (value >= 0xF8) {
			// Realtime message, send immediately.
			std::vector<uint8_t> realtimeMessage = { value };
			recvMessage(realtimeMessage, time);
			if (value == MIDI_MSG_RESET) {
				buffer.clear();
			}
			return;
		} else {
			// Replace any message in progress.
			if (isSysEx) {
				// Discarding incomplete MIDI SysEx message
			} else if (buffer.size() >= 2) {
				#if 0
				std::cerr << "Discarding incomplete MIDI message with status "
				             "0x" << std::hex << int(buffer[0]) << std::dec <<
				             ", at " << buffer.size() << " of " <<
				             midiMessageLength(buffer[0]) << " bytes\n";
				#endif
			}
			buffer = { value };
			isSysEx = value == MIDI_MSG_SYSEX;
		}
	} else { // data byte
		if (buffer.empty() && !isSysEx) {
			// Ignoring MIDI data without preceding status
		} else {
			buffer.push_back(value);
		}
	}

	// Is the message complete?
	if (!buffer.empty()) {
		uint8_t status = isSysEx ? MIDI_MSG_SYSEX : buffer[0];
		size_t len = midiMessageLength(status);
		if (buffer.size() >= len) {
			recvMessage(buffer, time);
			if (status >= 0xF0 && status < 0xF8) {
				buffer.clear();
			} else {
				// Keep last status, to support running status.
				buffer.resize(1);
			}
		}
	}
}

void MidiOutDevice::recvMessage(
		const std::vector<uint8_t>& /*message*/, EmuTime /*time*/)
{
	UNREACHABLE;
}

void MidiOutDevice::setDataBits(DataBits /*bits*/)
{
	// ignore
}

void MidiOutDevice::setStopBits(StopBits /*bits*/)
{
	// ignore
}

void MidiOutDevice::setParityBit(bool /*enable*/, Parity /*parity*/)
{
	// ignore
}

} // namespace openmsx
