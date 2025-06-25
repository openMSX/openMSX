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
	virtual void signal(EmuTime time) = 0;

	// SerialDataInterface (part) (output)
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;

	// control
	[[nodiscard]] virtual std::optional<bool> getCTS(EmuTime time) const;
	[[nodiscard]] virtual std::optional<bool> getDSR(EmuTime time) const;
	[[nodiscard]] virtual std::optional<bool> getDCD(EmuTime time) const;
	[[nodiscard]] virtual std::optional<bool> getRI(EmuTime time) const;
	virtual void setDTR(bool status, EmuTime time);
	virtual void setRTS(bool status, EmuTime time);
};

} // namespace openmsx

#endif
