#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

class PrinterPortDevice;

class MSXPrinterPort final : public MSXDevice, public Connector
{
public:
	explicit MSXPrinterPort(const DeviceConfig& config);

	PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	// Connector
	std::string_view getDescription() const override;
	std::string_view getClass() const override;
	void plug(Pluggable& dev, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(byte newData, EmuTime::param time);

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
