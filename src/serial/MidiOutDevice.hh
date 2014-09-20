#ifndef MIDIOUTDEVICE_HH
#define MIDIOUTDEVICE_HH

#include "Pluggable.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiOutDevice : public Pluggable, public SerialDataInterface
{
public:
	// Pluggable (part)
	string_ref getClass() const final override;

	// SerialDataInterface (part)
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
};

} // namespace openmsx

#endif
