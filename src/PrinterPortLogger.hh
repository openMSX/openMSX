// $Id$

#ifndef __MSXPRINTERPORTLOGGER_HH__
#define __MSXPRINTERPORTLOGGER_HH__

#include "PrinterPortDevice.hh"
#include "FileOpener.hh"


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
		virtual void plug(const EmuTime &time);
		virtual void unplug(const EmuTime &time);
		virtual const std::string &getName();

	private:
		byte toPrint;
		bool prevStrobe;
		IOFILETYPE* file;
};

#endif
