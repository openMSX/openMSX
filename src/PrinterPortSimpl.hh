#ifndef PRINTERPORTSIMPLE_HH
#define PRINTERPORTSIMPLE_HH

#include "PrinterPortDevice.hh"
#include "DACSound8U.hh"
#include <optional>

namespace openmsx {

class HardwareConfig;

class PrinterPortSimpl final : public PrinterPortDevice
{
public:
	explicit PrinterPortSimpl(HardwareConfig& hwConf);

	// PrinterPortDevice
	[[nodiscard]] bool getStatus(EmuTime time) override;
	void setStrobe(bool strobe, EmuTime time) override;
	void writeData(uint8_t data, EmuTime time) override;

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createDAC();

private:
	HardwareConfig& hwConf;
	std::optional<DACSound8U> dac;
};

} // namespace openmsx

#endif
