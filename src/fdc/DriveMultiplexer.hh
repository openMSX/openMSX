// $Id$

#ifndef DRIVEMULTIPLEXER_HH
#define DRIVEMULTIPLEXER_HH

#include "DiskDrive.hh"

namespace openmsx {

/**
 * This class connects to a FDC as a normal DiskDrive and deligates all
 * requests to one of four other DiskDrives.
 */
class DriveMultiplexer : public DiskDrive
{
public:
	enum DriveNum {
		DRIVE_A = 0,
		DRIVE_B = 1,
		DRIVE_C = 2,
		DRIVE_D = 3,
		NO_DRIVE = 4,
	};
	
	// Multiplexer interface
	DriveMultiplexer(DiskDrive* drive[4]);
	virtual ~DriveMultiplexer();
	void selectDrive(DriveNum num, const EmuTime &time);
	
	// DiskDrive interface
	virtual bool ready();
	virtual bool writeProtected();
	virtual bool doubleSided();
	virtual void setSide(bool side);
	virtual void step(bool direction, const EmuTime &time);
	virtual bool track00(const EmuTime &time);
	virtual void setMotor(bool status, const EmuTime &time);
	virtual bool indexPulse(const EmuTime &time);
	virtual int indexPulseCount(const EmuTime &begin,
	                            const EmuTime &end);
	virtual EmuTime getTimeTillSector(byte sector, const EmuTime& time);
	virtual void setHeadLoaded(bool status, const EmuTime &time);
	virtual bool headLoaded(const EmuTime &time);
	virtual void read (byte sector, byte* buf,
			   byte &onDiskTrack, byte &onDiskSector,
			   byte &onDiskSide,  int  &onDiskSize);
	virtual void write(byte sector, const byte* buf,
			   byte &onDiskTrack, byte &onDiskSector,
			   byte &onDiskSide,  int  &onDiskSize);
	virtual void getSectorHeader(byte sector, byte* buf);
	virtual void getTrackHeader(byte* buf);
	virtual void initWriteTrack();
	virtual void writeTrackData(byte data);
	virtual bool diskChanged();
	virtual bool dummyDrive();

private:
	DiskDrive* drive[5];
	DriveNum selected;
	bool motor;
	bool side;
};

} // namespace openmsx

#endif
