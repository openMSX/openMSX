// $Id$

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXIODevice.hh"
#include "PrinterPortDevice.hh"
#include "Connector.hh"

// forward declarations
class PrinterPortSimpl;
class PrinterPortLogger;


class DummyPrinterPortDevice : public PrinterPortDevice
{
	virtual bool getStatus(const EmuTime &time);
	virtual void setStrobe(bool strobe, const EmuTime &time);
	virtual void writeData(byte data, const EmuTime &time);
};


class MSXPrinterPort : public MSXIODevice , public Connector
{
	public:
		MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXPrinterPort();

		// MSXIODevice
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		
		// Connector
		virtual const std::string &getName() const;
		virtual const std::string &getClass() const;
		virtual void plug(Pluggable *dev, const EmuTime &time);
		virtual void unplug(const EmuTime &time);

	private:
		void setStrobe(bool newStrobe, const EmuTime &time);
		void writeData(byte newData, const EmuTime &time);

		DummyPrinterPortDevice *dummy;
		PrinterPortLogger *logger;
		PrinterPortSimpl *simple;
		
		bool strobe;
		byte data;
};

#endif
