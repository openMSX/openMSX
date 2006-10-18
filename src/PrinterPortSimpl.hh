// $Id$

#ifndef PRINTERPORTSIMPLE_HH
#define PRINTERPORTSIMPLE_HH

#include "PrinterPortDevice.hh"
#include <memory>

namespace openmsx {

class MSXMixer;
class DACSound8U;

class PrinterPortSimpl : public PrinterPortDevice
{
public:
	explicit PrinterPortSimpl(MSXMixer& mixer);

	// PrinterPortDevice
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

private:
	MSXMixer& mixer;
	std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
