// $Id$

#include "PrinterPortLogger.hh"
#include "FileContext.hh"
#include "File.hh"
#include <cassert>


namespace openmsx {

PrinterPortLogger::PrinterPortLogger()
	: logFilenameSetting("printerlogfilename",
		"filename of the file where the printer output is logged to",
		"printer.log")
{
	file = NULL;
	prevStrobe = true;
}

PrinterPortLogger::~PrinterPortLogger()
{
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
	throw(PlugException)
{
	// TODO: Add exception to File class and use it here.
	file = new File(logFilenameSetting.getValue(), TRUNCATE);
}

void PrinterPortLogger::unplug(const EmuTime &time)
{
	delete file;
	file = NULL;
}

const string& PrinterPortLogger::getName() const
{
	static const string name("logger");
	return name;
}

const string& PrinterPortLogger::getDescription() const
{
	static const string desc(
		"Log everything that is sent to the printer port to a "
		"file. The filename can be set with the "
		"'printerlogfilename' setting.");
	return desc;
}

} // namespace openmsx
