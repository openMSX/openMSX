#ifndef __MSXPRINTERLOGGER_HH__
#define __MSXPRINTERLOGGER_HH__

#include "MSXIODevice.hh"
#include "FileOpener.hh"

class MSXPrinterLogger : public MSXIODevice
{
	public:
		MSXPrinterLogger(MSXConfig::Device *config, const EmuTime &time);
		~MSXPrinterLogger();

		void reset(const EmuTime &time);

		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
	private:
		IOFILETYPE* file;
		byte data;
};
#endif

