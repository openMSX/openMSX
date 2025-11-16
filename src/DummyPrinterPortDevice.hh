#ifndef DUMMYPRINTERPORTDEVICE_HH
#define DUMMYPRINTERPORTDEVICE_HH

#include "PrinterPortDevice.hh"

namespace openmsx {

class DummyPrinterPortDevice final : public PrinterPortDevice
{
public:
	[[nodiscard]] bool getStatus(EmuTime time) override;
	void setStrobe(bool strobe, EmuTime time) override;
	void writeData(uint8_t data, EmuTime time) override;
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
};

} // namespace openmsx

#endif
