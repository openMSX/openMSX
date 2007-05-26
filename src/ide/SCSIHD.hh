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
#ifndef SCSIHD_HH
#define SCSIHD_HH

#include "HD.hh"
#include "SCSIDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class File;
class XMLElement;
class MSXMotherBoard;

class SCSIHD : public HD, public SCSIDevice, public SectorAccessibleDisk,
               public DiskContainer, private noncopyable
{
public:
	SCSIHD(MSXMotherBoard& motherBoard, const XMLElement& targetconfig, 
		byte* buf, const char* name, byte type, int mode);
	virtual ~SCSIHD();

	// SectorAccessibleDisk:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;

protected:
	// SCSI Device
	virtual void reset();
	virtual bool isSelected();
	virtual int executeCmd(const byte* cdb, SCSI::Phase& phase, int& blocks);
	virtual int executingCmd(SCSI::Phase& phase, int& blocks);
	virtual byte getStatusCode();
	virtual int msgOut(byte value);
	virtual byte msgIn();
	virtual void disconnect();
	virtual void busReset();

	virtual int dataIn(int& blocks);
	virtual int dataOut(int& blocks);

private:
	bool getReady();
	bool diskChanged();
	void testUnitReady();
	void startStopUnit();
	int inquiry();
	int modeSense();
	int requestSense();
	bool checkReadOnly();
	int readCapacity();
	bool checkAddress();
	int readSector(int& blocks);
	int writeSector(int& blocks);
	void formatUnit();

	MSXMotherBoard& motherBoard;

	const byte scsiId;     // SCSI ID 0..7
	const byte deviceType; // TODO only needed because we don't have seperate
	                       //  subclasses for the different devicetypes
	const int mode;
	const char* const productName;

	bool unitAttention;    // Unit Attention (was: reset)
	bool motor;            // Reserved
	int keycode;           // Sense key, ASC, ASCQ
	bool inserted;
	bool changed;          // Enhanced change flag for MEGA-SCSI driver
	bool changeCheck2;     // Disk change control flag
	unsigned currentSector;
	unsigned sectorSize;   // TODO should be a constant: 512 for HD, 2048 for CD
	unsigned currentLength;
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block
	byte* buffer;
};

} // namespace openmsx

#endif
