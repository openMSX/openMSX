#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskDrive.hh"
#include "DiskChanger.hh"
#include "Clock.hh"
#include "Schedulable.hh"
#include "ThrottleManager.hh"
#include "outer.hh"
#include "serialize_meta.hh"
#include <bitset>
#include <optional>

namespace openmsx {

class MSXMotherBoard;

/** This class implements a real drive, single or double sided.
 */
class RealDrive final : public DiskDrive
{
public:
	RealDrive(MSXMotherBoard& motherBoard, EmuDuration::param motorTimeout,
	          bool signalsNeedMotorOn, bool doubleSided,
	          DiskDrive::TrackMode trackMode);
	~RealDrive() override;

	// DiskDrive interface
	[[nodiscard]] bool isDiskInserted() const override;
	[[nodiscard]] bool isWriteProtected() const override;
	[[nodiscard]] bool isDoubleSided() override;
	[[nodiscard]] bool isTrack00() const override;
	void setSide(bool side) override;
	[[nodiscard]] bool getSide() const override;
	void step(bool direction, EmuTime::param time) override;
	void setMotor(bool status, EmuTime::param time) override;
	[[nodiscard]] bool getMotor() const override;
	[[nodiscard]] bool indexPulse(EmuTime::param time) override;
	[[nodiscard]] EmuTime getTimeTillIndexPulse(EmuTime::param time, int count) override;

	[[nodiscard]] unsigned getTrackLength() override;
	void writeTrackByte(int idx, byte val, bool addIdam) override;
	[[nodiscard]] byte  readTrackByte(int idx) override;
	EmuTime getNextSector(EmuTime::param time, RawTrack::Sector& sector) override;
	void flushTrack() override;
	bool diskChanged() override;
	[[nodiscard]] bool peekDiskChanged() const override;
	[[nodiscard]] bool isDummyDrive() const override;

	void applyWd2793ReadTrackQuirk() override;
	void invalidateWd2793ReadTrackQuirk() override;

	void serialize(Archive auto& ar, unsigned version);

private:
	struct SyncLoadingTimeout final : Schedulable {
		friend class RealDrive;
		explicit SyncLoadingTimeout(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param /*time*/) override {
			auto& drive = OUTER(RealDrive, syncLoadingTimeout);
			drive.execLoadingTimeout();
		}
	} syncLoadingTimeout;

	struct SyncMotorTimeout final : Schedulable {
		friend class RealDrive;
		explicit SyncMotorTimeout(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& drive = OUTER(RealDrive, syncMotorTimeout);
			drive.execMotorTimeout(time);
		}
	} syncMotorTimeout;

	void execLoadingTimeout();
	void execMotorTimeout(EmuTime::param time);
	[[nodiscard]] EmuTime::param getCurrentTime() const { return syncLoadingTimeout.getCurrentTime(); }

	void doSetMotor(bool status, EmuTime::param time);
	void setLoading(EmuTime::param time);
	[[nodiscard]] unsigned getCurrentAngle(EmuTime::param time) const;

	[[nodiscard]] unsigned getMaxTrack() const;
	[[nodiscard]] std::optional<unsigned> getDiskReadTrack() const;
	[[nodiscard]] std::optional<unsigned> getDiskWriteTrack() const;
	void getTrack();
	void invalidateTrack();

private:
	static constexpr unsigned TICKS_PER_ROTATION = 200000;
	static constexpr unsigned INDEX_DURATION = TICKS_PER_ROTATION / 50;

	MSXMotherBoard& motherBoard;
	LoadingIndicator loadingIndicator;
	const EmuDuration motorTimeout;

	using MotorClock = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>;
	MotorClock motorTimer;
	std::optional<DiskChanger> changer; // delayed initialization
	unsigned headPos;
	unsigned side;
	unsigned startAngle;
	bool motorStatus;
	const bool doubleSizedDrive;
	const bool signalsNeedMotorOn;
	const DiskDrive::TrackMode trackMode;

	static constexpr unsigned MAX_DRIVES = 26; // a-z
	using DrivesInUse = std::bitset<MAX_DRIVES>;
	std::shared_ptr<DrivesInUse> drivesInUse;

	RawTrack track;
	bool trackValid;
	bool trackDirty;
};
SERIALIZE_CLASS_VERSION(RealDrive, 6);

} // namespace openmsx

#endif
