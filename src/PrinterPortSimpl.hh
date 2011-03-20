// $Id$

#ifndef PRINTERPORTSIMPLE_HH
#define PRINTERPORTSIMPLE_HH

#include "PrinterPortDevice.hh"
#include "serialize_meta.hh"
#include <memory>

namespace openmsx {

class MSXMixer;
class DACSound8U;

class PrinterPortSimpl : public PrinterPortDevice
{
public:
	explicit PrinterPortSimpl(MSXMixer& mixer);

	// PrinterPortDevice
	virtual bool getStatus(EmuTime::param time);
	virtual void setStrobe(bool strobe, EmuTime::param time);
	virtual void writeData(byte data, EmuTime::param time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createDAC();

	MSXMixer& mixer;
	std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
