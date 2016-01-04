#ifndef MIDIOUTDEVICE_HH
#define MIDIOUTDEVICE_HH

#include "Pluggable.hh"
#include "SerialDataInterface.hh"
#include <vector>


namespace openmsx {

/** Pluggable that connects an MSX MIDI out port to a host MIDI device.
  * Implementations must override either recvByte(), if they can handle raw
  * data, or recvMessage(), if they handle MIDI messages.
  */
class MidiOutDevice : public Pluggable, public SerialDataInterface
{
public:
	/** The limit for the amount of data we'll put into one MIDI message.
	  * The value of 256 comes from the data limit for a MIDIPacket in CoreMIDI.
	  */
	static constexpr size_t MAX_MESSAGE_SIZE = 256;

	// Pluggable (part)
	string_ref getClass() const final override;

	// SerialDataInterface (part)
	void recvByte(byte value, EmuTime::param time) override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;

protected:
	explicit MidiOutDevice();

	/** Discard any buffered partial MIDI message.
	  */
	void clearBuffer();

	/** Called when a full MIDI message is ready to be sent.
	  * Implementations that can handle raw bytes can override recvByte()
	  * instead, in which case this method will not be called.
	  */
	virtual void recvMessage(
			const std::vector<uint8_t>& message, EmuTime::param time);

private:
	std::vector<uint8_t> buffer;
	bool isSysEx;
};

} // namespace openmsx

#endif
