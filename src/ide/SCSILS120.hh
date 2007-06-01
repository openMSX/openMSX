// $Id$
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
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class XMLElement;
class MSXMotherBoard;
class LSXCommand;

class SCSILS120 : public SCSIDevice, public SectorAccessibleDisk,
               public DiskContainer, private noncopyable
{
public:
	SCSILS120(MSXMotherBoard& motherBoard, const XMLElement& targetconfig, 
		byte* const buf, unsigned mode);
	virtual ~SCSILS120();

	// SectorAccessibleDisk:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;

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

private:
	bool getReady();
	bool diskChanged();
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

	const byte scsiId;     // SCSI ID 0..7
	const int mode;

	bool unitAttention;    // Unit Attention (was: reset)
	int keycode;           // Sense key, ASC, ASCQ
	bool mediaChanged;     // Enhanced change flag for MEGA-SCSI driver
	unsigned currentSector;
	unsigned currentLength;
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block
	byte* const buffer;

	std::auto_ptr<File> file;

	std::string name;
	std::auto_ptr<LSXCommand> lsxCommand;

	friend class LSXCommand;
};

} // namespace openmsx

#endif
