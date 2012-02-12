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
	virtual bool isDiskInserted() const;
	virtual bool isWriteProtected() const;
	virtual bool isDoubleSided() const;
	virtual bool isTrack00() const;
	virtual void setSide(bool side);
	virtual void step(bool direction, EmuTime::param time);
	virtual void setMotor(bool status, EmuTime::param time);
	virtual bool indexPulse(EmuTime::param time);
	virtual EmuTime getTimeTillSector(byte sector, EmuTime::param time);
	virtual EmuTime getTimeTillIndexPulse(EmuTime::param time);
	virtual void setHeadLoaded(bool status, EmuTime::param time);
	virtual bool headLoaded(EmuTime::param time);
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
	virtual bool isDummyDrive() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Timer stuff, needed for the notification of the loading state
	virtual void executeUntil(EmuTime::param time, int userData);

	// This is all for the ThrottleManager
	void resetTimeOut(EmuTime::param time);
	void updateLoadingState();

	static const int MAX_TRACK = 85;
	static const int TICKS_PER_ROTATION = 6850; // TODO
	static const int ROTATIONS_PER_SECOND = 5;
	static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;

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
