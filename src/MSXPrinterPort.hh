// $Id$

#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "Connector.hh"

namespace openmsx {

class PrinterPortDevice;

class MSXPrinterPort : public MSXDevice, public Connector
{
public:
	MSXPrinterPort(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXPrinterPort();

	PrinterPortDevice& getPluggedPrintDev() const;

	// MSXDevice
	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual void plug(Pluggable& dev, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setStrobe(bool newStrobe, const EmuTime& time);
	void writeData(byte newData, const EmuTime& time);

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
