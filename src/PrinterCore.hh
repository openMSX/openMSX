#ifndef PRINTER_CORE_HH
#define PRINTER_CORE_HH

#include "PrinterPortDevice.hh"

namespace openmsx {

// Abstract printer class
class PrinterCore : public PrinterPortDevice
{
public:
	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime time) override;
	void setStrobe(bool strobe, EmuTime time) override;
	void writeData(uint8_t data, EmuTime time) override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

protected:
	PrinterCore() = default;
	~PrinterCore() override = default;
	virtual void write(uint8_t data) = 0;
	virtual void forceFormFeed() = 0;

private:
	uint8_t toPrint = 0;
	bool prevStrobe = true;
};

} // namespace openmsx

#endif
