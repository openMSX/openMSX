// $Id$

#ifndef IDECDROM_HH
#define IDECDROM_HH

#include "AbstractIDEDevice.hh"

namespace openmsx {

class EventDistributor;
class XMLElement;

class IDECDROM : public AbstractIDEDevice
{
public:
	IDECDROM(EventDistributor& eventDistributor, const XMLElement& config,
	      const EmuTime& time);
	virtual ~IDECDROM();

protected:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual void readBlockStart(byte* buffer);
	virtual void writeBlockComplete(byte* buffer);
	virtual void executeCommand(byte cmd);

private:
	void executePacketCommand(byte* packet);
};

} // namespace openmsx

#endif
