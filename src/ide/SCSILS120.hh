/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.h,v
** Revision: 1.6
** Date: 2007-05-22 20:05:38 +0200 (Tue, 22 May 2007)
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
#ifndef SCSILS120_HH
#define SCSILS120_HH

#include "HD.hh"
#include "SCSIDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "serialize_meta.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class DeviceConfig;
class MSXMotherBoard;
class LSXCommand;

class SCSILS120 : public SCSIDevice, public SectorAccessibleDisk,
                  public DiskContainer, private noncopyable
{
public:
	SCSILS120(const DeviceConfig& targetconfig,
	          AlignedBuffer& buf, unsigned mode);
	virtual ~SCSILS120();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SectorAccessibleDisk:
	virtual void readSectorImpl (size_t sector,       SectorBuffer& buf);
	virtual void writeSectorImpl(size_t sector, const SectorBuffer& buf);
	virtual size_t getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;
	virtual Sha1Sum getSha1Sum();

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;
	virtual bool diskChanged();
	virtual int insertDisk(const std::string& filename);

	// SCSI Device
	virtual void reset();
	virtual bool isSelected();
	virtual unsigned executeCmd(const byte* cdb, SCSI::Phase& phase, unsigned& blocks);
	virtual unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks);
	virtual byte getStatusCode();
	virtual int msgOut(byte value);
	virtual byte msgIn();
	virtual void disconnect();
	virtual void busReset();
	virtual void eject();
	virtual void insert(const std::string& filename);

	virtual unsigned dataIn(unsigned& blocks);
	virtual unsigned dataOut(unsigned& blocks);

	bool getReady();
	void testUnitReady();
	void startStopUnit();
	unsigned inquiry();
	unsigned modeSense();
	unsigned requestSense();
	bool checkReadOnly();
	unsigned readCapacity();
	bool checkAddress();
	unsigned readSector(unsigned& blocks);
	unsigned writeSector(unsigned& blocks);
	void formatUnit();

	MSXMotherBoard& motherBoard;
	AlignedBuffer& buffer;
	std::unique_ptr<File> file;
	std::unique_ptr<LSXCommand> lsxCommand;
	std::string name;
	const int mode;
	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;
	const byte scsiId;     // SCSI ID 0..7
	bool unitAttention;    // Unit Attention (was: reset)
	bool mediaChanged;     // Enhanced change flag for MEGA-SCSI driver
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block

	friend class LSXCommand;
};

} // namespace openmsx

#endif
