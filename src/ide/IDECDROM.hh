// $Id$

#ifndef IDECDROM_HH
#define IDECDROM_HH

#include "AbstractIDEDevice.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class File;
class CDXCommand;

class IDECDROM : public AbstractIDEDevice
{
public:
	IDECDROM(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time);
	virtual ~IDECDROM();

protected:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual unsigned readBlockStart(byte* buffer, unsigned count);
	virtual void readEnd();
	virtual void writeBlockComplete(byte* buffer, unsigned count);
	virtual void executeCommand(byte cmd);

private:
	// Flags for the interrupt reason register:
	/** Bus release: 0 = normal, 1 = bus release */
	static const byte REL = 0x04;
	/** I/O direction: 0 = host->device, 1 = device->host */
	static const byte I_O = 0x02;
	/** Command/data: 0 = data, 1 = command */
	static const byte C_D = 0x01;

	/** Indicates the start of a read data transfer performed in packets.
	  * @param count Total number of bytes to transfer.
	  */
	void startPacketReadTransfer(unsigned count);

	void executePacketCommand(byte* packet);

	const std::string name;
	const std::auto_ptr<CDXCommand> cdxCommand;
	std::auto_ptr<File> file;
	unsigned byteCountLimit;
	bool readSectorData;
	unsigned transferOffset;

	friend class CDXCommand;
};

} // namespace openmsx

#endif
