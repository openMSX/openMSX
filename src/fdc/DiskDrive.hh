// $Id$

#ifndef __DISKDRIVE_HH__
#define __DISKDRIVE_HH__

#include <string>
#include "EmuTime.hh"
#include "Command.hh"

class FDCBackEnd;


/**
 * This (abstract) class defines the DiskDrive interface
 */
class DiskDrive
{
	public:
		virtual ~DiskDrive();
	
		/** Is drive ready
		 */
		virtual bool ready() = 0;

		/** Is disk write protected
		 */
		virtual bool writeProtected() = 0;

		/** Is disk double sided
		 */
		virtual bool doubleSided() = 0;
		
		/** Side select
		 * @param side false = side 0
		 *             true  = side 1
		 */
		virtual void setSide(bool side) = 0;

		/** Step head
		 * @param direction false = out
		 *                  true  = in
		 */
		virtual void step(bool direction, const EmuTime &time) = 0;

		/** Head above track 0
		 */
		virtual bool track00(const EmuTime &time) = 0;

		/** Set motor on/off
		 * @param status false = off
		 *               true  = on
		 */
		virtual void setMotor(bool status, const EmuTime &time) = 0;

		/** Index pulse at this moment
		 */
		virtual bool indexPulse(const EmuTime &time) = 0;

		/** How many index pulses between these two time stamps
		 */
		virtual int indexPulseCount(const EmuTime &begin,
		                            const EmuTime &end) = 0;

		/** Set head loaded status
		 * @param status false = not loaded
		 *               true  = loaded
		 */
		virtual void setHeadLoaded(bool status, const EmuTime &time) = 0;

		/** Is head loaded
		 */
		virtual bool headLoaded(const EmuTime &time) = 0;

		// TODO
		// Read / write methods, mostly copied from FDCBackEnd,
		// but needs to be reworked
		virtual void read (byte sector, byte* buf,
		                   byte &onDiskTrack, byte &onDiskSector,
		                   byte &onDiskSide,  int  &onDiskSize) = 0;
		virtual void write(byte sector, const byte* buf,
		                   byte &onDiskTrack, byte &onDiskSector,
		                   byte &onDiskSide,  int  &onDiskSize) = 0;
		virtual void getSectorHeader(byte sector, byte* buf) = 0;
		virtual void getTrackHeader(byte track, byte* buf) = 0;
		virtual void initWriteTrack(byte track) = 0;
		virtual void writeTrackData(byte data) = 0;

		// high level read / write methods
		virtual void readSector(byte* buf, int sector);
		virtual void writeSector(const byte* buf, int sector);
};


/**
 * This class implements a not connected disk drive
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
		virtual void getTrackHeader(byte track, byte* buf);
		virtual void initWriteTrack(byte track);
		virtual void writeTrackData(byte data);
};


/**
 * This class implements a real drive, this is the parent class for both
 * sigle and double sided drives. Common methods are implemented here;
 */
class RealDrive : public DiskDrive, public Command
{
	public:
		RealDrive(const std::string &drivename, const EmuTime &time);
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

	protected:
		static const int MAX_TRACK = 85;
		static const int TICKS_PER_ROTATION = 10000;	// TODO
		static const int ROTATIONS_PER_SECOND = 5;
		static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;
	
		FDCBackEnd* disk;
		int headPos;
		bool motorStatus;
		EmuTimeFreq<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> motorTime;
		bool headLoadStatus;
		EmuTimeFreq<1000> headLoadTime;

	private:
		// Command interface
		virtual void execute(const std::vector<std::string> &tokens);
		virtual void help   (const std::vector<std::string> &tokens);
		virtual void tabCompletion(std::vector<std::string> &tokens);
		void insertDisk(const std::string &disk);
		void ejectDisk();

		std::string name;
};


/**
 * This class implements a sigle sided drive
 */
class SingleSidedDrive : public RealDrive
{
	public:
		SingleSidedDrive(const std::string &drivename,
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
		virtual void getTrackHeader(byte track, byte* buf);
		virtual void initWriteTrack(byte track);
		virtual void writeTrackData(byte data);
};


/**
 * This class implements a double sided drive
 */
class DoubleSidedDrive : public RealDrive
{
	public:
		DoubleSidedDrive(const std::string &drivename,
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
		virtual void getTrackHeader(byte track, byte* buf);
		virtual void initWriteTrack(byte track);
		virtual void writeTrackData(byte data);
		virtual void readSector(byte* buf, int sector);
		virtual void writeSector(const byte* buf, int sector);
	
	private:
		int side;
};


#endif
