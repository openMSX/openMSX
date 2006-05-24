// $Id$

#ifndef IDECDROM_HH
#define IDECDROM_HH

#include "AbstractIDEDevice.hh"

namespace openmsx {

class CommandController;
class EventDistributor;
class XMLElement;

class IDECDROM : public AbstractIDEDevice
{
public:
	IDECDROM(
		CommandController& commandController,
		EventDistributor& eventDistributor,
		const XMLElement& config,
		const EmuTime& time
		);
	virtual ~IDECDROM();

protected:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual void readBlockStart(byte* buffer);
	virtual void readEnd();
	virtual void writeBlockComplete(byte* buffer);
	virtual void executeCommand(byte cmd);

private:
	// Flags for the interrupt reason register:
	/** Bus release: 0 = normal, 1 = bus release */
	static const byte REL = 0x04;
	/** I/O direction: 0 = host->device, 1 = device->host */
	static const byte I_O = 0x02;
	/** Command/data: 0 = data, 1 = command */
	static const byte C_D = 0x01;

	void executePacketCommand(byte* packet);
};

} // namespace openmsx

#endif
