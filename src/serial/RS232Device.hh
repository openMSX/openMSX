// $Id$

#ifndef __RS232DEVICE_HH__
#define __RS232DEVICE_HH__

#include "Pluggable.hh"
#include "SerialDataInterface.hh"


class RS232Device : public Pluggable, public SerialDataInterface
{
	public:
		// Pluggable (part)
		virtual const string& getClass() const;
		
		// input
		virtual void signal(const EmuTime& time) = 0;
		
		// SerialDataInterface (part) (output)
		virtual void setDataBits(DataBits bits);
		virtual void setStopBits(StopBits bits);
		virtual void setParityBit(bool enable, ParityBit parity);

		// control
		virtual bool getCTS(const EmuTime& time) const;
		virtual bool getDSR(const EmuTime& time) const;
		virtual void setDTR(bool status, const EmuTime& time);
		virtual void setRTS(bool status, const EmuTime& time);
};

#endif

