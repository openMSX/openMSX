// $Id$

#ifndef __DISKDRIVE_HH__
#define __DISKDRIVE_HH__

#include <string>
#include "EmuTime.hh"
#include "Command.hh"

namespace openmsx {

class Disk;
class FileContext;


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
	virtual void step(bool direction, const EmuTime &time) = 0;

	/** Head above track 0
	 */
	virtual bool track00(const EmuTime &time) = 0;

	/** Set motor on/off
	 * @param status false = off,
	 *               true  = on.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setMotor(bool status, const EmuTime &time) = 0;

	/** Gets the state of the index pulse.
	 * @param time The moment in emulated time to get the pulse state for.
	 */
	virtual bool indexPulse(const EmuTime &time) = 0;

	/** Count the number index pulses in an interval.
	 * @param begin Begin time of interval.
	 * @param end End time of interval.
	 * @return The number of index pulses between "begin" and "end".
	 */
	virtual int indexPulseCount(const EmuTime &begin,
				    const EmuTime &end) = 0;

	/** Set head loaded status.
	 * @param status false = not loaded,
	 *               true  = loaded.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setHeadLoaded(bool status, const EmuTime &time) = 0;

	/** Is head loaded?
	 */
	virtual bool headLoaded(const EmuTime &time) = 0;

	// TODO
	// Read / write methods, mostly copied from Disk,
	// but needs to be reworked
	virtual void read (byte sector, byte* buf,
			   byte &onDiskTrack, byte &onDiskSector,
			   byte &onDiskSide,  int  &onDiskSize) = 0;
	virtual void write(byte sector, const byte* buf,
			   byte &onDiskTrack, byte &onDiskSector,
			   byte &onDiskSide,  int  &onDiskSize) = 0;
	virtual void getSectorHeader(byte sector, byte* buf) = 0;
	virtual void getTrackHeader(byte* buf) = 0;
	virtual void initWriteTrack() = 0;
	virtual void writeTrackData(byte data) = 0;

	/** Is disk changed?
	 */
	virtual bool diskChanged() = 0;

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
	virtual void step(bool direction, const EmuTime &time);
	virtual bool track00(const EmuTime &time);
	virtual void setMotor(bool status, const EmuTime &time);
	virtual bool indexPulse(const EmuTime &time);
	virtual int indexPulseCount(const EmuTime &begin,
				    const EmuTime &end);
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
};


/**
 * This class implements a real drive, this is the parent class for both
 * sigle and double sided drives. Common methods are implemented here;
 */
class RealDrive : public DiskDrive, public SimpleCommand
{
public:
	RealDrive(const string &drivename, const EmuTime &time);
	virtual ~RealDrive();

	// DiskDrive interface
	virtual bool ready();
	virtual bool writeProtected();
	virtual void step(bool direction, const EmuTime &time);
	virtual bool track00(const EmuTime &time);
	virtual void setMotor(bool status, const EmuTime &time);
	virtual bool indexPulse(const EmuTime &time);
	virtual int indexPulseCount(const EmuTime &begin,
				    const EmuTime &end);
	virtual void setHeadLoaded(bool status, const EmuTime &time);
	virtual bool headLoaded(const EmuTime &time);
	virtual bool diskChanged();

protected:
	static const int MAX_TRACK = 85;
	static const int TICKS_PER_ROTATION = 6850;	// TODO
	static const int ROTATIONS_PER_SECOND = 5;
	static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;

	Disk* disk;
	int headPos;
	bool motorStatus;
	EmuTimeFreq<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> motorTime;
	bool headLoadStatus;
	EmuTimeFreq<1000> headLoadTime;	// ms

private:
	// Command interface
	virtual string execute(const vector<string> &tokens)
		throw(CommandException);
	virtual string help   (const vector<string> &tokens) const
		throw();
	virtual void tabCompletion(vector<string> &tokens) const
		throw();
	void insertDisk(FileContext &context,
			const string &disk);
	void ejectDisk();

	string name;
	string diskName;
	bool diskChangedFlag;
};


/**
 * This class implements a sigle sided drive.
 */
class SingleSidedDrive : public RealDrive
{
public:
	SingleSidedDrive(const string &drivename,
			 const EmuTime &time);
	virtual ~SingleSidedDrive();
	virtual bool doubleSided();
	virtual void setSide(bool side);
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
};


/**
 * This class implements a double sided drive.
 */
class DoubleSidedDrive : public RealDrive
{
public:
	DoubleSidedDrive(const string &drivename,
			 const EmuTime &time);
	virtual ~DoubleSidedDrive();
	virtual bool doubleSided();
	virtual void setSide(bool side);
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

	// high level read / write methods used by DiskRomPatch
	void readSector(byte* buf, int sector);
	void writeSector(const byte* buf, int sector);

private:
	int side;
};

} // namespace openmsx


#endif
