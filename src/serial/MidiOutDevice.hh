// $Id$

#ifndef __MIDIOUTDEVICE_HH__
#define __MIDIOUTDEVICE_HH__

#include "Pluggable.hh"
#include "SerialDataInterface.hh"


class MidiOutDevice : public Pluggable, public SerialDataInterface
{
	public:
		// Pluggable (part)
		virtual const string& getClass() const;

		// SerialDataInterface (part)
		virtual void setDataBits(DataBits bits);
		virtual void setStopBits(StopBits bits);
		virtual void setParityBit(bool enable, ParityBit parity);
};

#endif

