#ifndef SVIPRINTERPORT_HH
#define SVIPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"

namespace openmsx {

class PrinterPortDevice;

class SVIPrinterPort final : public MSXDevice, public Connector
{
public:
	SVIPrinterPort(const DeviceConfig& config);

	[[nodiscard]] PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// Connector
	[[nodiscard]] std::string_view getDescription() const override;
	[[nodiscard]] std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(byte newData, EmuTime::param time);

private:
	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
