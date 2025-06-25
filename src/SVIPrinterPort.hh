#ifndef SVIPRINTERPORT_HH
#define SVIPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"
#include <cstdint>

namespace openmsx {

class PrinterPortDevice;

class SVIPrinterPort final : public MSXDevice, public Connector
{
public:
	explicit SVIPrinterPort(const DeviceConfig& config);

	[[nodiscard]] PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, EmuTime time);
	void writeData(uint8_t newData, EmuTime time);

private:
	bool strobe = false; // != true
	uint8_t data = 255;  // != 0
};

} // namespace openmsx

#endif
