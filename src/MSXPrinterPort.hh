// $Id$

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXIODevice.hh"
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
	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};


class MSXPrinterPort : public MSXIODevice, public Connector
{
public:
	MSXPrinterPort(Config* config, const EmuTime& time);
	virtual ~MSXPrinterPort();

	// MSXIODevice
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	// Connector
	virtual const string& getDescription() const;
	virtual const string& getClass() const;
	virtual void plug(Pluggable* dev, const EmuTime& time);
	virtual PrinterPortDevice& getPlugged() const;

private:
	void setStrobe(bool newStrobe, const EmuTime& time);
	void writeData(byte newData, const EmuTime& time);

	bool strobe;
	byte data;
};

} // namespace openmsx

#endif // __MSXPRINTERPORT_HH__
