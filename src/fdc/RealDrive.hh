// $Id$

#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskDrive.hh"
#include "Clock.hh"
#include "Schedulable.hh"
#include <memory>

namespace openmsx {

class DiskChanger;
class CommandController;
class EventDistributor;
class MSXEventDistributor;
class Scheduler;
class FileManipulator;
class ThrottleManager;
class LoadingIndicator;

/**
 * This class implements a real drive, this is the parent class for both
 * sigle and double sided drives. Common methods are implemented here;
 */
class RealDrive : public DiskDrive, public Schedulable
{
public:
	static const int MAX_DRIVES = 26;	// a-z

	RealDrive(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          MSXEventDistributor& msxEventDistributor,
	          Scheduler& scheduler,
	          FileManipulator& fileManipulator,
	          const EmuTime& time);
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
	virtual bool peekDiskChanged() const;
	virtual bool dummyDrive();

	// Timer stuff, needed for the notification of the loading state
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

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

	EventDistributor& eventDistributor;
private:
	// This is all for the ThrottleManager
	void resetTimeOut(const EmuTime& time);
	void updateLoadingState();
	CommandController& commandController;
	std::auto_ptr<LoadingIndicator> loadingIndicator;
	bool timeOut;
};


/**
 * This class implements a sigle sided drive.
 */
class SingleSidedDrive : public RealDrive
{
public:
	SingleSidedDrive(CommandController& commandController,
	                 EventDistributor& eventDistributor,
	                 MSXEventDistributor& msxEventDistributor,
	                 Scheduler& scheduler,
	                 FileManipulator& fileManipulator,
	                 const EmuTime& time);
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
	DoubleSidedDrive(CommandController& commandController,
	                 EventDistributor& eventDistributor,
	                 MSXEventDistributor& msxEventDistributor,
	                 Scheduler& scheduler,
	                 FileManipulator& fileManipulator,
	                 const EmuTime& time);
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
