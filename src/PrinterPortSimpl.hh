#ifndef PRINTERPORTSIMPLE_HH
#define PRINTERPORTSIMPLE_HH

#include "PrinterPortDevice.hh"
#include <memory>

namespace openmsx {

class HardwareConfig;
class DACSound8U;

class PrinterPortSimpl final : public PrinterPortDevice
{
public:
	explicit PrinterPortSimpl(const HardwareConfig& hwConf);
	~PrinterPortSimpl() override;

	// PrinterPortDevice
	bool getStatus(EmuTime::param time) override;
	void setStrobe(bool strobe, EmuTime::param time) override;
	void writeData(byte data, EmuTime::param time) override;

	// Pluggable
	const std::string& getName() const override;
	std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createDAC();

	const HardwareConfig& hwConf;
	std::unique_ptr<DACSound8U> dac; // can be nullptr
};

} // namespace openmsx

#endif
