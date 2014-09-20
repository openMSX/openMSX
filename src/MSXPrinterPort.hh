#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"

namespace openmsx {

class PrinterPortDevice;

class MSXPrinterPort final : public MSXDevice, public Connector
{
public:
	explicit MSXPrinterPort(const DeviceConfig& config);
	~MSXPrinterPort();

	PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	// Connector
	virtual const std::string getDescription() const;
	virtual string_ref getClass() const;
	virtual void plug(Pluggable& dev, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, EmuTime::param time);
	void writeData(byte newData, EmuTime::param time);

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
