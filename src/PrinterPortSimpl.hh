// $Id$

#ifndef __PRINTERPORTSIMPLE_HH__
#define __PRINTERPORTSIMPLE_HH__

#include "PrinterPortDevice.hh"
class DACSound;


class PrinterPortSimpl : public PrinterPortDevice
{
	public:
		PrinterPortSimpl();
		virtual ~PrinterPortSimpl();

		// PrinterPortDevice
		virtual bool getStatus(const EmuTime &time);
		virtual void setStrobe(bool strobe, const EmuTime &time);
		virtual void writeData(byte data, const EmuTime &time);
		
		// Pluggable
		virtual const std::string &getName();
		virtual void plug(const EmuTime &time);
		virtual void unplug(const EmuTime &time);
		
	private:
		DACSound* dac;

		static const std::string name;
};
#endif

