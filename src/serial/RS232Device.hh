// $Id$

#ifndef RS232DEVICE_HH
#define RS232DEVICE_HH

#include "Pluggable.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class RS232Device : public Pluggable, public SerialDataInterface
{
public:
	// Pluggable (part)
	virtual const std::string& getClass() const;
	
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

} // namespace openmsx

#endif
