// $Id: 

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXDevice.hh"
#include "PrinterPortDevice.hh"


class MSXPrinterPort : public MSXDevice
{
	public:
		MSXPrinterPort();
		~MSXPrinterPort();

		void init();
		
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
		
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

