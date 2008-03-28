// $Id$

#ifndef DUMMYPRINTERPORTDEVICE_HH
#define DUMMYPRINTERPORTDEVICE_HH

#include "PrinterPortDevice.hh"

namespace openmsx {

class DummyPrinterPortDevice : public PrinterPortDevice
{
public:
	virtual bool getStatus(const EmuTime& time);
	virtual void setStrobe(bool strobe, const EmuTime& time);
	virtual void writeData(byte data, const EmuTime& time);
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
