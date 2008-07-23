// $Id$

#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskDrive.hh"
#include "Clock.hh"
#include "Schedulable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class DiskChanger;
class LoadingIndicator;

/** This class implements a real drive, single or double sided.
 */
class RealDrive : public DiskDrive, public Schedulable
{
public:
	RealDrive(MSXMotherBoard& motherBoard, bool doubleSided);
	virtual ~RealDrive();

	// DiskDrive interface
	virtual bool ready();
	virtual bool writeProtected();
	virtual bool doubleSided();
	virtual void setSide(bool side);
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Timer stuff, needed for the notification of the loading state
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	// This is all for the ThrottleManager
	void resetTimeOut(const EmuTime& time);
	void updateLoadingState();

	static const int MAX_TRACK = 85;
	static const int TICKS_PER_ROTATION = 6850;	// TODO
	static const int ROTATIONS_PER_SECOND = 5;
	static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	std::auto_ptr<LoadingIndicator> loadingIndicator;

	Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> motorTimer;
	Clock<1000> headLoadTimer; // ms
	std::auto_ptr<DiskChanger> changer;
	int headPos;
	int side;
	bool motorStatus;
	bool headLoadStatus;
	bool timeOut;
	const bool doubleSizedDrive;
};

} // namespace openmsx

#endif
