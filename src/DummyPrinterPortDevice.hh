#ifndef DUMMYPRINTERPORTDEVICE_HH
#define DUMMYPRINTERPORTDEVICE_HH

#include "PrinterPortDevice.hh"

namespace openmsx {

class DummyPrinterPortDevice final : public PrinterPortDevice
{
public:
	[[nodiscard]] bool getStatus(EmuTime::param time) override;
	void setStrobe(bool strobe, EmuTime::param time) override;
	void writeData(byte data, EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif
