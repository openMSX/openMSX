// $Id$

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXIODevice.hh"
#include "PrinterPortDevice.hh"
#include "Connector.hh"

// forward declarations
class PrinterPortSimple;


class LoggingPrinterPortDevice : public PrinterPortDevice
{
public:
	LoggingPrinterPortDevice();
	virtual ~LoggingPrinterPortDevice();

	virtual bool getStatus(const EmuTime &time);
	virtual void setStrobe(bool strobe, const EmuTime &time);
	virtual void writeData(byte data, const EmuTime &time);
	virtual const std::string &getName();
private:
	byte toPrint;
	static const std::string name;
};

class DummyPrinterPortDevice : public PrinterPortDevice
{
	virtual bool getStatus(const EmuTime &time);
	virtual void setStrobe(bool strobe, const EmuTime &time);
	virtual void writeData(byte data, const EmuTime &time);
	virtual const std::string &getName();
	static const std::string name;
};


class MSXPrinterPort : public MSXIODevice , public Connector
{
	public:
		MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time);
		~MSXPrinterPort();

		void reset(const EmuTime &time);

		// MSXIODevice
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
		
		// Connector
		virtual const std::string &getName();
		virtual const std::string &getClass();
		virtual void plug(Pluggable *dev, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

	private:
		DummyPrinterPortDevice *dummy;
		LoggingPrinterPortDevice *logger;
		PrinterPortSimple *simple;
		
		bool strobe;
		byte data;

		static const std::string name;
		static const std::string className;
};

#endif
