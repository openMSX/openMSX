// $Id$

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXIODevice.hh"
#include "PrinterPortDevice.hh"


class MSXPrinterPort : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		~MSXPrinterPort();

		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);
		
		void plug(PrinterPortDevice *dev);
		void unplug();

	private:
		PrinterPortDevice *device;
		PrinterPortDevice *dummy;
		bool strobe;
		byte data;
};


class DummyPrinterPortDevice : public PrinterPortDevice
{
	bool getStatus();
	void setStrobe(bool strobe);
	void writeData(byte data);
};

#endif

