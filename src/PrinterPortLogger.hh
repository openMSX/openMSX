// $Id$

#ifndef __MSXPRINTERPORTLOGGER_HH__
#define __MSXPRINTERPORTLOGGER_HH__

#include <memory>
#include "PrinterPortDevice.hh"
#include "StringSetting.hh"

namespace openmsx {

class File;

class PrinterPortLogger : public PrinterPortDevice
{
public:
	PrinterPortLogger();
	virtual ~PrinterPortLogger();

	// PrinterPortDevice
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

private:
	byte toPrint;
	bool prevStrobe;
	std::auto_ptr<File> file;

	StringSetting logFilenameSetting;
};

} // namespace openmsx

#endif // __MSXPRINTERPORTLOGGER_HH__
