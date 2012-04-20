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
	virtual EmuTime getTimeTillIndexPulse(EmuTime::param time);
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
	virtual void executeUntil(EmuTime::param time, int userData);
	void doSetMotor(bool status, EmuTime::param time);
	void setLoading(EmuTime::param time);
	unsigned getCurrentAngle(EmuTime::param time) const;

	static const unsigned MAX_TRACK = 85;
	static const unsigned TICKS_PER_ROTATION = 200000;
	static const unsigned INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;
	const EmuDuration motorTimeout;

	typedef Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> MotorClock;
	MotorClock motorTimer;
	Clock<1000> headLoadTimer; // ms
	std::auto_ptr<DiskChanger> changer;
	unsigned headPos;
	unsigned side;
	unsigned startAngle;
	bool motorStatus;
	bool headLoadStatus;
	const bool doubleSizedDrive;
};
SERIALIZE_CLASS_VERSION(RealDrive, 3);

} // namespace openmsx

#endif
