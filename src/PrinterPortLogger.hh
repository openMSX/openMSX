// $Id$

#ifndef __MSXPRINTERPORTLOGGER_HH__
#define __MSXPRINTERPORTLOGGER_HH__

#include "PrinterPortDevice.hh"

namespace openmsx {

class File;


class PrinterPortLogger : public PrinterPortDevice
{
	public:
		PrinterPortLogger();
		virtual ~PrinterPortLogger();

		// PrinterPortDevice
		virtual bool getStatus(const EmuTime &time);
		virtual void setStrobe(bool strobe, const EmuTime &time);
		virtual void writeData(byte data, const EmuTime &time);
		
		// Pluggable
		virtual const string &getName() const;
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime& time);

	private:
		byte toPrint;
		bool prevStrobe;
		File* file;
};

} // namespace openmsx

#endif
