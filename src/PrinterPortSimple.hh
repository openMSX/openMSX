// $Id$

#ifndef __MSXSIMPLE_HH__
#define __MSXSIMPLE_HH__

#include "PrinterPortDevice.hh"
class DACSound;


class PrinterPortSimple : public PrinterPortDevice
{
	public:
		PrinterPortSimple();
		virtual ~PrinterPortSimple();

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

