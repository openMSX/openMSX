#ifndef RS232DEVICE_HH
#define RS232DEVICE_HH

#include "Pluggable.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class RS232Device : public Pluggable, public SerialDataInterface
{
public:
	// Pluggable (part)
	[[nodiscard]] std::string_view getClass() const final;

	// input
	virtual void signal(EmuTime::param time) = 0;

	// SerialDataInterface (part) (output)
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;

	// control
	[[nodiscard]] virtual bool getCTS(EmuTime::param time) const;
	[[nodiscard]] virtual bool getDSR(EmuTime::param time) const;
	[[nodiscard]] virtual bool getDCD(EmuTime::param time) const;
	virtual void setDTR(bool status, EmuTime::param time);
	virtual void setRTS(bool status, EmuTime::param time);
};

} // namespace openmsx

#endif
