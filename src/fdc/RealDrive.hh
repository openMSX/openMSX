#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskDrive.hh"
#include "Clock.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
#include <bitset>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class DiskChanger;
class LoadingIndicator;

/** This class implements a real drive, single or double sided.
 */
class RealDrive final : public DiskDrive, public Schedulable
{
public:
	RealDrive(MSXMotherBoard& motherBoard, EmuDuration::param motorTimeout,
	          bool signalsNeedMotorOn, bool doubleSided);
	~RealDrive();

	// DiskDrive interface
	bool isDiskInserted() const override;
	bool isWriteProtected() const override;
	bool isDoubleSided() const override;
	bool isTrack00() const override;
	void setSide(bool side) override;
	void step(bool direction, EmuTime::param time) override;
	void setMotor(bool status, EmuTime::param time) override;
	bool indexPulse(EmuTime::param time) override;
	EmuTime getTimeTillIndexPulse(EmuTime::param time, int count) override;
	void setHeadLoaded(bool status, EmuTime::param time) override;
	bool headLoaded(EmuTime::param time) override;
	void writeTrack(const RawTrack& track) override;
	void readTrack (      RawTrack& track) override;
	EmuTime getNextSector(EmuTime::param time, RawTrack& track,
	                      RawTrack::Sector& sector) override;
	bool diskChanged() override;
	bool peekDiskChanged() const override;
	bool isDummyDrive() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void executeUntil(EmuTime::param time, int userData) override;
	void doSetMotor(bool status, EmuTime::param time);
	void setLoading(EmuTime::param time);
	unsigned getCurrentAngle(EmuTime::param time) const;

	static const unsigned MAX_TRACK = 85;
	static const unsigned TICKS_PER_ROTATION = 200000;
	static const unsigned INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	const std::unique_ptr<LoadingIndicator> loadingIndicator;
	const EmuDuration motorTimeout;

	typedef Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND> MotorClock;
	MotorClock motorTimer;
	Clock<1000> headLoadTimer; // ms
	std::unique_ptr<DiskChanger> changer;
	unsigned headPos;
	unsigned side;
	unsigned startAngle;
	bool motorStatus;
	bool headLoadStatus;
	const bool doubleSizedDrive;
	const bool signalsNeedMotorOn;

	static const unsigned MAX_DRIVES = 26; // a-z
	typedef std::bitset<MAX_DRIVES> DrivesInUse;
	std::shared_ptr<DrivesInUse> drivesInUse;
};
SERIALIZE_CLASS_VERSION(RealDrive, 3);

} // namespace openmsx

#endif
