// $Id$

#ifndef IDEHD_HH
#define IDEHD_HH

#include "HD.hh"
#include "AbstractIDEDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "noncopyable.hh"

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class DiskManipulator;

class IDEHD : public HD, public AbstractIDEDevice, public SectorAccessibleDisk,
              public DiskContainer, private noncopyable
{
public:
	IDEHD(MSXMotherBoard& motherBoard, const XMLElement& config,
	      const EmuTime& time);
	virtual ~IDEHD();

	// SectorAccessibleDisk:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;

protected:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual unsigned readBlockStart(byte* buffer, unsigned count);
	virtual void writeBlockComplete(byte* buffer, unsigned count);
	virtual void executeCommand(byte cmd);

private:
	DiskManipulator& diskManipulator;
	unsigned transferSectorNumber;
};

} // namespace openmsx

#endif
