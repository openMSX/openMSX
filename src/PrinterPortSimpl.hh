// $Id$

#ifndef __PRINTERPORTSIMPLE_HH__
#define __PRINTERPORTSIMPLE_HH__

#include "PrinterPortDevice.hh"

namespace openmsx {

class DACSound8U;


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
		virtual const string &getName() const;
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime &time);
		
	private:
		DACSound8U* dac;
};

} // namespace openmsx

#endif

