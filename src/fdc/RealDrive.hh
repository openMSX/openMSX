// $Id$

#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskDrive.hh"
#include "Clock.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
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
	RealDrive(MSXMotherBoard& motherBoard, EmuDuration::param motorTimeout,
	          bool doubleSided);
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
	virtual void executeUntil(EmuTime::param time, int userData);
	void doSetMotor(bool status, EmuTime::param time);
	void setLoading(EmuTime::param time);

	static const int MAX_TRACK = 85;
	static const int TICKS_PER_ROTATION = 6250; // see Disk.hh
	static const int ROTATIONS_PER_SECOND = 5;
	static const int INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;
	const EmuDuration motorTimeout;

	typedef Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> MotorClock;
	MotorClock motorTimer;
	Clock<1000> headLoadTimer; // ms
	std::auto_ptr<DiskChanger> changer;
	int headPos;
	int side;
	bool motorStatus;
	bool headLoadStatus;
	const bool doubleSizedDrive;
};
SERIALIZE_CLASS_VERSION(RealDrive, 2);

} // namespace openmsx

#endif
