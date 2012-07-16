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
		NO_DRIVE = 4
	};

	// Multiplexer interface
	explicit DriveMultiplexer(DiskDrive* drive[4]);
	virtual ~DriveMultiplexer();
	void selectDrive(DriveNum num, EmuTime::param time);

	// DiskDrive interface
	virtual bool isDiskInserted() const;
	virtual bool isWriteProtected() const;
	virtual bool isDoubleSided() const;
	virtual bool isTrack00() const;
	virtual void setSide(bool side);
	virtual void step(bool direction, EmuTime::param time);
	virtual void setMotor(bool status, EmuTime::param time);
	virtual bool indexPulse(EmuTime::param time);
	virtual EmuTime getTimeTillIndexPulse(EmuTime::param time, int count);
	virtual void setHeadLoaded(bool status, EmuTime::param time);
	virtual bool headLoaded(EmuTime::param time);
	virtual void writeTrack(const RawTrack& track);
	virtual void readTrack (      RawTrack& track);
	virtual EmuTime getNextSector(EmuTime::param time, RawTrack& track,
	                              RawTrack::Sector& sector);
	virtual bool diskChanged();
	virtual bool peekDiskChanged() const;
	virtual bool isDummyDrive() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	DiskDrive* drive[5];
	DriveNum selected;
	bool motor;
	bool side;
};

} // namespace openmsx

#endif
