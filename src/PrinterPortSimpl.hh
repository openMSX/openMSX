// $Id$

#ifndef __PRINTERPORTSIMPLE_HH__
#define __PRINTERPORTSIMPLE_HH__

#include <memory>
#include "PrinterPortDevice.hh"

using std::auto_ptr;

namespace openmsx {

class DACSound8U;

class PrinterPortSimpl : public PrinterPortDevice
{
public:
	PrinterPortSimpl();
	virtual ~PrinterPortSimpl();

	// PrinterPortDevice
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);

	// Pluggable
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

private:
	auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif

