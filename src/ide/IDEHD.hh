// $Id$

#ifndef IDEHD_HH
#define IDEHD_HH

#include "HD.hh"
#include "AbstractIDEDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "serialize_meta.hh"
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SectorAccessibleDisk:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual unsigned getNbSectorsImpl() const;

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;

	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual unsigned readBlockStart(byte* buffer, unsigned count);
	virtual void writeBlockComplete(byte* buffer, unsigned count);
	virtual void executeCommand(byte cmd);

	// SectorAccessibleDisk
	virtual bool isWriteProtectedImpl() const;

	DiskManipulator& diskManipulator;
	unsigned transferSectorNumber;
};

REGISTER_POLYMORPHIC_INITIALIZER(IDEDevice, IDEHD, "IDEHD");

} // namespace openmsx

#endif
