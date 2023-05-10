#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"
#include "SimpleDebuggable.hh"
#include <cstdint>

namespace openmsx {

class PrinterPortDevice;

class MSXPrinterPort final : public MSXDevice, public Connector
{
public:
	explicit MSXPrinterPort(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime::param time) override;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] PrinterPortDevice& getPluggedPrintDev() const;
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(uint8_t newData, EmuTime::param time);

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value) override;
	} debuggable;

	bool strobe = false; // != true
	uint8_t data = 255;  // != 0
	const uint8_t writePortMask;
	const uint8_t readPortMask;
	const uint8_t unusedBits;
};

} // namespace openmsx

#endif
