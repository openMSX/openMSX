// $Id$

#ifndef __MSXPRINTERPORT_HH__
#define __MSXPRINTERPORT_HH__

#include "MSXIODevice.hh"
#include "PrinterPortDevice.hh"
#include "ConsoleSource/ConsoleCommand.hh"

class LoggingPrinterPortDevice : public PrinterPortDevice
{
	bool getStatus();
	void setStrobe(bool strobe, const EmuTime &time);
	void writeData(byte data, const EmuTime &time);
private:
	byte toPrint;
};

class DummyPrinterPortDevice : public PrinterPortDevice
{
	bool getStatus();
	void setStrobe(bool strobe, const EmuTime &time);
	void writeData(byte data, const EmuTime &time);
};

class MSXPrinterPort : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time);
		static MSXPrinterPort *instance();
		/**
		 * Destructor
		 */
		~MSXPrinterPort();

		void reset(const EmuTime &time);

		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
		
		void plug(PrinterPortDevice *dev, const EmuTime &time);
		void unplug(const EmuTime &time);
		static LoggingPrinterPortDevice *logger;

	private:
		static MSXPrinterPort *oneInstance;
		PrinterPortDevice *device;
		DummyPrinterPortDevice *dummy;
		bool strobe;
		byte data;
		// ConsoleCommands
		class printPortCmd : public ConsoleCommand {
		public:
			printPortCmd();
			virtual ~printPortCmd();
			virtual void execute(const char *commandLine);
			virtual void help(const char *commandLine);
		};

		printPortCmd printPortCmd ;

};

#endif

