#ifndef DUMMYPRINTERPORTDEVICE_HH
#define DUMMYPRINTERPORTDEVICE_HH

#include "PrinterPortDevice.hh"

namespace openmsx {

class DummyPrinterPortDevice : public PrinterPortDevice
{
public:
	virtual bool getStatus(EmuTime::param time);
	virtual void setStrobe(bool strobe, EmuTime::param time);
	virtual void writeData(byte data, EmuTime::param time);
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
};

} // namespace openmsx

#endif
