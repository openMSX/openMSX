// $Id$

#include "PrinterPortLogger.hh"
#include "PluggingController.hh"
#include "FileContext.hh"
#include "File.hh"
#include <cassert>


namespace openmsx {

PrinterPortLogger::PrinterPortLogger()
{
	file = NULL;
	prevStrobe = true;
	PluggingController::instance()->registerPluggable(this);
}

PrinterPortLogger::~PrinterPortLogger()
{
	PluggingController::instance()->unregisterPluggable(this);
	delete file;
}

bool PrinterPortLogger::getStatus(const EmuTime &time)
{
	return false;	// false = low = ready
}

void PrinterPortLogger::setStrobe(bool strobe, const EmuTime &time)
{
	assert(file);
	PRT_DEBUG("PRINTER: strobe " << strobe);
	if (!strobe && prevStrobe) {
		// falling edge
		PRT_DEBUG("PRINTER: save in printlog file " << toPrint);
		file->write(&toPrint, 1);
	}
	prevStrobe = strobe;
}

void PrinterPortLogger::writeData(byte data, const EmuTime &time)
{
	PRT_DEBUG("PRINTER: setting data " << data);
	toPrint = data;
}

void PrinterPortLogger::plug(Connector* connector, const EmuTime& time)
{
	const string filename("printer.log");	// TODO read from config
	file = new File(filename, TRUNCATE);
}

void PrinterPortLogger::unplug(const EmuTime &time)
{
	delete file;
	file = NULL;
}

const string &PrinterPortLogger::getName() const
{
	static const string name("logger");
	return name;
}

} // namespace openmsx
