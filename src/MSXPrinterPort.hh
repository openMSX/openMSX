// $Id$

#ifndef MSXPRINTERPORT_HH
#define MSXPRINTERPORT_HH

#include "MSXDevice.hh"
#include "PrinterPortDevice.hh"
#include "Connector.hh"

namespace openmsx {

class DummyPrinterPortDevice : public PrinterPortDevice
{
public:
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);

protected:
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};


class MSXPrinterPort : public MSXDevice, public Connector
{
public:
	MSXPrinterPort(const XMLElement& config, const EmuTime& time);
	virtual ~MSXPrinterPort();

	// MSXDevice
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual void plug(Pluggable* dev, const EmuTime& time);
	virtual PrinterPortDevice& getPlugged() const;

private:
	void setStrobe(bool newStrobe, const EmuTime& time);
	void writeData(byte newData, const EmuTime& time);

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif
