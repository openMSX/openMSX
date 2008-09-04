// $Id$

#ifndef DISKDRIVE_HH
#define DISKDRIVE_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include "openmsx.hh"

namespace openmsx {

/**
 * This (abstract) class defines the DiskDrive interface
 */
class DiskDrive : private noncopyable
{
public:
	virtual ~DiskDrive();

	/** Is drive ready?
	 */
	virtual bool isReady() const = 0;

	/** Is disk write protected?
	 */
	virtual bool isWriteProtected() const = 0;

	/** Is disk double sided?
	 */
	virtual bool isDoubleSided() const = 0;

	/** Head above track 0
	 */
	virtual bool isTrack00() const = 0;

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
	virtual void writeTrackData(const byte* data) = 0;

	/** Is disk changed?
	 */
	virtual bool diskChanged() = 0;
	virtual bool peekDiskChanged() const = 0;

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
	virtual bool isReady() const;
	virtual bool isWriteProtected() const;
	virtual bool isDoubleSided() const;
	virtual bool isTrack00() const;
	virtual void setSide(bool side);
	virtual void step(bool direction, const EmuTime& time);
	virtual void setMotor(bool status, const EmuTime& time);
	virtual bool indexPulse(const EmuTime& time);
	virtual int indexPulseCount(const EmuTime& begin,
	                            const EmuTime& end);
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
	virtual void writeTrackData(const byte* data);
	virtual bool diskChanged();
	virtual bool peekDiskChanged() const;
	virtual bool dummyDrive();
};

} // namespace openmsx

#endif
