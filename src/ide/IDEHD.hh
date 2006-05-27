// $Id$

#ifndef IDEHD_HH
#define IDEHD_HH

#include "AbstractIDEDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include <memory>

namespace openmsx {

class EventDistributor;
class XMLElement;
class File;

class IDEHD : public AbstractIDEDevice, public SectorAccessibleDisk,
	public DiskContainer
{
public:
	IDEHD(EventDistributor& eventDistributor, const XMLElement& config,
	      const EmuTime& time);
	virtual ~IDEHD();

	// SectorAccessibleDisk:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// Diskcontainer:
	SectorAccessibleDisk* getSectorAccessibleDisk();

protected:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual unsigned readBlockStart(byte* buffer, unsigned count);
	virtual void writeBlockComplete(byte* buffer, unsigned count);
	virtual void executeCommand(byte cmd);

private:
	std::auto_ptr<File> file;
	unsigned totalSectors;
	unsigned transferSectorNumber;
};

} // namespace openmsx

#endif
