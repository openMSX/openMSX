#ifndef DRIVEMULTIPLEXER_HH
#define DRIVEMULTIPLEXER_HH

#include "DiskDrive.hh"
#include "serialize_meta.hh"

namespace openmsx {

/**
 * This class connects to a FDC as a normal DiskDrive and deligates all
 * requests to one of four other DiskDrives.
 */
class DriveMultiplexer final : public DiskDrive
{
public:
	enum DriveNum {
		DRIVE_A = 0,
		DRIVE_B = 1,
		DRIVE_C = 2,
		DRIVE_D = 3,
		NO_DRIVE = 4
	};

	// Multiplexer interface
	explicit DriveMultiplexer(DiskDrive* drv[4]);

	void selectDrive(DriveNum num, EmuTime::param time);
	[[nodiscard]] DriveNum getSelectedDrive() const { return selected; }

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

	[[nodiscard]] bool isDiskInserted(DriveNum num) const;
	bool diskChanged(DriveNum num);
	[[nodiscard]] bool peekDiskChanged(DriveNum num) const;

	void serialize(Archive auto& ar, unsigned version);

private:
	DummyDrive dummyDrive;
	DiskDrive* drive[5];
	DriveNum selected;
	bool motor;
	bool side;
};

} // namespace openmsx

#endif
