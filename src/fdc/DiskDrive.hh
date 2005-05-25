// $Id$

#ifndef DISKDRIVE_HH
#define DISKDRIVE_HH

#include "Clock.hh"
#include <memory>

namespace openmsx {

class SectorAccessibleDisk;
class DiskChanger;
class Disk;
class XMLElement;

/**
 * This (abstract) class defines the DiskDrive interface
 */
class DiskDrive
{
public:
	virtual ~DiskDrive();

	/** Is drive ready?
	 */
	virtual bool ready() = 0;

	/** Is disk write protected?
	 */
	virtual bool writeProtected() = 0;

	/** Is disk double sided?
	 */
	virtual bool doubleSided() = 0;

	/** Side select.
	 * @param side false = side 0,
	 *             true  = side 1.
	 */
	virtual void setSide(bool side) = 0;

	/** Step head
	 * @param direction false = out,
	 *                  true  = in.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void step(bool direction, const EmuTime& time) = 0;

	/** Head above track 0
	 */
	virtual bool track00(const EmuTime& time) = 0;

	/** Set motor on/off
	 * @param status false = off,
	 *               true  = on.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setMotor(bool status, const EmuTime& time) = 0;

	/** Gets the state of the index pulse.
	 * @param time The moment in emulated time to get the pulse state for.
	 */
	virtual bool indexPulse(const EmuTime& time) = 0;

	/** Count the number index pulses in an interval.
	 * @param begin Begin time of interval.
	 * @param end End time of interval.
	 * @return The number of index pulses between "begin" and "end".
	 */
	virtual int indexPulseCount(const EmuTime& begin,
	                            const EmuTime& end) = 0;

	/** Return the time when the indicated sector will be rotated under
	 * the drive head.
	 * TODO what when the requested sector is not present? For the moment
	 * returns the current time.
	 * @param sector The requested sector number
	 * @param time The current time
	 * @return Time when the requested sector is under the drive head.
	 */
	virtual EmuTime getTimeTillSector(byte sector, const EmuTime& time) = 0;
	
	/** Return the time till the start of the next index pulse
	 * When there is no disk in the drive or when the disk is not spinning,
	 * this function returns the current time.
	 * @param time The current time
	 */
	virtual EmuTime getTimeTillIndexPulse(const EmuTime& time) = 0;
	
	/** Set head loaded status.
	 * @param status false = not loaded,
	 *               true  = loaded.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setHeadLoaded(bool status, const EmuTime& time) = 0;

	/** Is head loaded?
	 */
	virtual bool headLoaded(const EmuTime& time) = 0;

	// TODO
	// Read / write methods, mostly copied from Disk,
	// but needs to be reworked
	virtual void read (byte sector, byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int& onDiskSize) = 0;
	virtual void write(byte sector, const byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int& onDiskSize) = 0;
	virtual void getSectorHeader(byte sector, byte* buf) = 0;
	virtual void getTrackHeader(byte* buf) = 0;
	virtual void initWriteTrack() = 0;
	virtual void writeTrackData(byte data) = 0;

	/** Is disk changed?
	 */
	virtual bool diskChanged() = 0;

	/** Is there a dummy (unconncted) drive?
	 */
	virtual bool dummyDrive() = 0;
};


/**
 * This class implements a not connected disk drive.
 */
class DummyDrive : public DiskDrive
{
public:
	DummyDrive();
	virtual ~DummyDrive();
	virtual bool ready();
	virtual bool writeProtected();
	virtual bool doubleSided();
	virtual void setSide(bool side);
	virtual void step(bool direction, const EmuTime& time);
	virtual bool track00(const EmuTime& time);
	virtual void setMotor(bool status, const EmuTime& time);
	virtual bool indexPulse(const EmuTime& time);
	virtual int indexPulseCount(const EmuTime &begin,
	                            const EmuTime &end);
	virtual EmuTime getTimeTillSector(byte sector, const EmuTime& time);
	virtual EmuTime getTimeTillIndexPulse(const EmuTime& time);
	virtual void setHeadLoaded(bool status, const EmuTime& time);
	virtual bool headLoaded(const EmuTime& time);
	virtual void read (byte sector, byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int&  onDiskSize);
	virtual void write(byte sector, const byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int&  onDiskSize);
	virtual void getSectorHeader(byte sector, byte* buf);
	virtual void getTrackHeader(byte* buf);
	virtual void initWriteTrack();
	virtual void writeTrackData(byte data);
	virtual bool diskChanged();
	virtual bool dummyDrive();
};


/**
 * This class implements a real drive, this is the parent class for both
 * sigle and double sided drives. Common methods are implemented here;
 */
class RealDrive : public DiskDrive
{
public:
	static const int MAX_DRIVES = 26;	// a-z

	RealDrive(const EmuTime& time);
	virtual ~RealDrive();

	// DiskDrive interface
	virtual bool ready();
	virtual bool writeProtected();
	virtual void step(bool direction, const EmuTime& time);
	virtual bool track00(const EmuTime& time);
	virtual void setMotor(bool status, const EmuTime& time);
	virtual bool indexPulse(const EmuTime& time);
	virtual int indexPulseCount(const EmuTime& begin,
	                            const EmuTime& end);
	virtual EmuTime getTimeTillSector(byte sector, const EmuTime& time);
	virtual EmuTime getTimeTillIndexPulse(const EmuTime& time);
	virtual void setHeadLoaded(bool status, const EmuTime& time);
	virtual bool headLoaded(const EmuTime& time);
	virtual bool diskChanged();
	virtual bool dummyDrive();

protected:
	static const int MAX_TRACK = 85;
	static const int TICKS_PER_ROTATION = 6850;	// TODO
	static const int ROTATIONS_PER_SECOND = 5;
	static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;

	int headPos;
	bool motorStatus;
	Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> motorTimer;
	bool headLoadStatus;
	Clock<1000> headLoadTimer; // ms
	std::auto_ptr<DiskChanger> changer;
};


/**
 * This class implements a sigle sided drive.
 */
class SingleSidedDrive : public RealDrive
{
public:
	SingleSidedDrive(const EmuTime& time);
	virtual ~SingleSidedDrive();
	virtual bool doubleSided();
	virtual void setSide(bool side);
	virtual void read (byte sector, byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int&  onDiskSize);
	virtual void write(byte sector, const byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int&  onDiskSize);
	virtual void getSectorHeader(byte sector, byte* buf);
	virtual void getTrackHeader(byte* buf);
	virtual void initWriteTrack();
	virtual void writeTrackData(byte data);
};


/**
 * This class implements a double sided drive.
 */
class DoubleSidedDrive : public RealDrive
{
public:
	DoubleSidedDrive(const EmuTime& time);
	virtual ~DoubleSidedDrive();
	virtual bool doubleSided();
	virtual void setSide(bool side);
	virtual void read (byte sector, byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int& onDiskSize);
	virtual void write(byte sector, const byte* buf,
			   byte& onDiskTrack, byte& onDiskSector,
			   byte& onDiskSide,  int&  onDiskSize);
	virtual void getSectorHeader(byte sector, byte* buf);
	virtual void getTrackHeader(byte* buf);
	virtual void initWriteTrack();
	virtual void writeTrackData(byte data);

private:
	int side;
};

} // namespace openmsx

#endif
