// $Id$

#ifndef MIDIOUTDEVICE_HH
#define MIDIOUTDEVICE_HH

#include "Pluggable.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiOutDevice : public Pluggable, public SerialDataInterface
{
public:
	// Pluggable (part)
	virtual string_ref getClass() const;

	// SerialDataInterface (part)
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
};

} // namespace openmsx

#endif
