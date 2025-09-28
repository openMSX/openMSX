#ifndef REALDRIVE_HH
#define REALDRIVE_HH

#include "DiskChanger.hh"
#include "DiskDrive.hh"

#include "Clock.hh"
#include "MSXMotherBoard.hh"
#include "Schedulable.hh"
#include "ThrottleManager.hh"
#include "serialize_meta.hh"

#include "outer.hh"

#include <bitset>
#include <optional>

namespace openmsx {

/** This class implements a real drive, single or double sided.
 */
class RealDrive final : public DiskDrive, public MediaProvider
{
public:
	static constexpr unsigned MAX_DRIVES = 26; // a-z
	using DrivesInUse = std::bitset<MAX_DRIVES>;
	static std::shared_ptr<DrivesInUse> getDrivesInUse(MSXMotherBoard& motherBoard);

public:
	RealDrive(MSXMotherBoard& motherBoard, EmuDuration motorTimeout,
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
	void step(bool direction, EmuTime time) override;
	void setMotor(bool status, EmuTime time) override;
	[[nodiscard]] bool getMotor() const override;
	[[nodiscard]] bool indexPulse(EmuTime time) override;
	[[nodiscard]] EmuTime getTimeTillIndexPulse(EmuTime time, int count) override;

	[[nodiscard]] unsigned getTrackLength() override;
	void writeTrackByte(int idx, uint8_t val, bool addIdam) override;
	[[nodiscard]] uint8_t readTrackByte(int idx) override;
	EmuTime getNextSector(EmuTime time, RawTrack::Sector& sector) override;
	void flushTrack() override;
	bool diskChanged() override;
	[[nodiscard]] bool peekDiskChanged() const override;
	[[nodiscard]] bool isDummyDrive() const override;

	void applyWd2793ReadTrackQuirk() override;
	void invalidateWd2793ReadTrackQuirk() override;

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;
	void setMedia(const TclObject& info, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct SyncLoadingTimeout final : Schedulable {
		friend class RealDrive;
		explicit SyncLoadingTimeout(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime /*time*/) override {
			auto& drive = OUTER(RealDrive, syncLoadingTimeout);
			drive.execLoadingTimeout();
		}
	} syncLoadingTimeout;

	struct SyncMotorTimeout final : Schedulable {
		friend class RealDrive;
		explicit SyncMotorTimeout(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& drive = OUTER(RealDrive, syncMotorTimeout);
			drive.execMotorTimeout(time);
		}
	} syncMotorTimeout;

	void execLoadingTimeout();
	void execMotorTimeout(EmuTime time);
	[[nodiscard]] EmuTime getCurrentTime() const { return syncLoadingTimeout.getCurrentTime(); }

	void doSetMotor(bool status, EmuTime time);
	void setLoading(EmuTime time);
	[[nodiscard]] unsigned getCurrentAngle(EmuTime time) const;

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
	unsigned headPos = 0;
	unsigned side = 0;
	unsigned startAngle = 0;
	bool motorStatus = false;
	const bool doubleSizedDrive;
	const bool signalsNeedMotorOn;
	const DiskDrive::TrackMode trackMode;

	std::shared_ptr<DrivesInUse> drivesInUse;

	RawTrack track;
	bool trackValid = false;
	bool trackDirty = false;
};
SERIALIZE_CLASS_VERSION(RealDrive, 6);

} // namespace openmsx

#endif
