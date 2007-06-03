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

class XMLElement;
class MSXMotherBoard;

class SCSIHD : public HD, public SCSIDevice, public SectorAccessibleDisk,
               public DiskContainer, private noncopyable
{
public:
	SCSIHD(MSXMotherBoard& motherBoard, const XMLElement& targetconfig,
		byte* const buf, unsigned mode);
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
	virtual unsigned executeCmd(const byte* cdb, SCSI::Phase& phase,
	                            unsigned& blocks);
	virtual unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks);
	virtual byte getStatusCode();
	virtual int msgOut(byte value);
	virtual byte msgIn();
	virtual void disconnect();
	virtual void busReset();

	virtual unsigned dataIn(unsigned& blocks);
	virtual unsigned dataOut(unsigned& blocks);

private:
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
	const unsigned mode;

	bool unitAttention;    // Unit Attention (was: reset)
	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block
	byte* const buffer;
};

} // namespace openmsx

#endif
