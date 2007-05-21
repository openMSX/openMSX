/*****************************************************************************
** $Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.h,v $
**
** $Revision: 1.6 $
**
** $Date$
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/
#ifndef SCSIDEVICE_HH
#define SCSIDEVICE_HH

// NOTE: THIS IS NOW CALLED SCSIDEVICE, BUT:
// - SCSIDevice should be a base class
// - this class has the CDROM support removed (should end up in SCSICDROM class)
// - this class is more like a SCSIHD class.
// So, TODO: split it up properly

//TODO: #include "ArchCdrom.h"
#include "openmsx.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>

#define BIT_SCSI2           0x0001
#define BIT_SCSI2_ONLY      0x0002
#define BIT_SCSI3           0x0004

#define MODE_SCSI1          0x0000
#define MODE_SCSI2          0x0003
#define MODE_SCSI3          0x0005
#define MODE_UNITATTENTION  0x0008  // report unit attention
#define MODE_MEGASCSI       0x0010  // report disk change when call of
                                    // 'test unit ready'
#define MODE_FDS120         0x0020  // change of inquiry when inserted floppy image
#define MODE_CHECK2         0x0040  // mask to diskchange when load state
#define MODE_REMOVABLE      0x0080
#define MODE_NOVAXIS        0x0100

#define BUFFER_SIZE         0x10000                 // 64KB
#define BUFFER_BLOCK_SIZE   (BUFFER_SIZE / 512)

namespace openmsx {

class File;

enum SCSI_PHASE {
    BusFree     =  0,
    Arbitration =  1,
    Selection   =  2,
    Reselection =  3,
    Command     =  4,
    Execute     =  5,
    DataIn      =  6,
    DataOut     =  7,
    Status      =  8,
    MsgOut      =  9,
    MsgIn       = 10
};

class SCSIDevice : public SectorAccessibleDisk, public DiskContainer, private noncopyable
{
public:
	SCSIDevice(int scsiId, byte* buf, char* name, int type, int mode
		/*TODO:, CdromXferCompCb xferCompCb */);
	virtual ~SCSIDevice();

	// SectorAccessibleDisk:
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// Diskcontainer:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;

	// SCSI Device
	void reset();
	bool isSelected();
	int executeCmd(byte* cdb, SCSI_PHASE* phase, int* blocks);
	int executingCmd(SCSI_PHASE* phase, int* blocks);
	byte getStatusCode();
	int msgOut(byte value);
	byte msgIn();
	void disconnect();
	void busReset();

	int dataIn(int* blocks);
	int dataOut(int* blocks);
	void enable(bool enable);

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
//	int openMSX(); // some kind of special command
	bool checkAddress();
	int readSector(int* blocks);
	int writeSector(int* blocks);
	void formatUnit();

	int scsiId;             // SCSI ID 0..7
	int deviceType;
	int mode;
	bool enabled;
	bool unitAttention;              // Unit Attention (was: reset)
	bool motor;              // Reserved
	int keycode;            // Sense key, ASC, ASCQ
	bool inserted;
	bool changed;            // Enhanced change flag for MEGA-SCSI driver
	bool changeCheck2;       // Disk change control flag
	int currentSector;
	int sectorSize;
	int currentLength;
	int message;
	int lun;
//TODO	ArchCdrom* cdrom;
	byte cdb[12];          // Command Descriptor Block
	byte* buffer;
	char* productName;
//TODO	FileProperties disk;

	std::auto_ptr<File> file;
	std::string name;
};

} // namespace openmsx

#endif
