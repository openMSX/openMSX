#include <cassert>
#include "MSXPrinterLogger.hh"
#include "MSXMotherBoard.hh"

MSXPrinterLogger::MSXPrinterLogger(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXPrinterLogger");

	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);
	MSXMotherBoard::instance()->register_IO_In (0x91, this);

	std::string filename = config->getParameter("filename");
	file = FileOpener::openFileTruncate(filename);

	reset(time);
}

MSXPrinterLogger::~MSXPrinterLogger()
{
	delete file;
}

void MSXPrinterLogger::reset(const EmuTime &time)
{
}


byte MSXPrinterLogger::readIO(byte port, const EmuTime &time)
{
	switch (port) {
	  case 0x90:
		// printer ready ? bit 1=status, other bits are 1
		return 0xfd;
		break;
	  case 0x91:
		// normally you don't read data from printer but it is possible!
		return 0;
		break;
	  default:
		assert(false);
	}
}

void MSXPrinterLogger::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0x90:
		// printer only prints after a strobe signal :-) ?
		// bit 0 is strobe
		if (value & 1) file->write(&data,1);
		break;
	case 0x91:
		data=value;
		break;
	default:
		assert(false);
	}
}
