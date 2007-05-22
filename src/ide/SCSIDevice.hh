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

#include "SCSI.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>

namespace openmsx {

class File;

class SCSIDevice : public SectorAccessibleDisk, public DiskContainer, private noncopyable
{
public:
	static const int BIT_SCSI2          = 0x0001;
	static const int BIT_SCSI2_ONLY     = 0x0002;
	static const int BIT_SCSI3          = 0x0004;

	static const int MODE_SCSI1         = 0x0000;
	static const int MODE_SCSI2         = 0x0003;
	static const int MODE_SCSI3         = 0x0005;
	static const int MODE_UNITATTENTION = 0x0008; // report unit attention
	static const int MODE_MEGASCSI      = 0x0010; // report disk change when call of
	                                              // 'test unit ready'
	static const int MODE_FDS120        = 0x0020; // change of inquiry when inserted
	                                              // floppy image
	static const int MODE_CHECK2        = 0x0040; // mask to diskchange when
	                                              // load state
	static const int MODE_REMOVABLE     = 0x0080;
	static const int MODE_NOVAXIS       = 0x0100;

	static const int BUFFER_SIZE        = 0x10000; // 64KB

	SCSIDevice(byte scsiId, byte* buf, const char* name, byte type, int mode);
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
	int executeCmd(const byte* cdb, SCSI::Phase& phase, int& blocks);
	int executingCmd(SCSI::Phase& phase, int& blocks);
	byte getStatusCode();
	int msgOut(byte value);
	byte msgIn();
	void disconnect();
	void busReset(); // TODO not used?

	int dataIn(int& blocks);
	int dataOut(int& blocks);
	void enable(bool enable); // TODO not used?

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

	const byte scsiId;     // SCSI ID 0..7
	const byte deviceType; // TODO only needed because we don't have seperate
	                       //  subclasses for the different devicetypes
	const int mode;
	const char* const productName;

	bool enabled;
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

	std::auto_ptr<File> file;
	std::string name;
};

} // namespace openmsx

#endif
