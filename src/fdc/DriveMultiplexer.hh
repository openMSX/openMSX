#ifndef DRIVEMULTIPLEXER_HH
#define DRIVEMULTIPLEXER_HH

#include "DiskDrive.hh"

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
	DriveNum getSelectedDrive() const { return selected; }

	// DiskDrive interface
	bool isDiskInserted() const override;
	bool isWriteProtected() const override;
	bool isDoubleSided() const override;
	bool isTrack00() const override;
	void setSide(bool side) override;
	bool getSide() const override;
	void step(bool direction, EmuTime::param time) override;
	void setMotor(bool status, EmuTime::param time) override;
	bool getMotor() const override;
	bool indexPulse(EmuTime::param time) override;
	EmuTime getTimeTillIndexPulse(EmuTime::param time, int count) override;
	unsigned getTrackLength() override;
	void writeTrackByte(int idx, byte val, bool addIdam) override;
	byte  readTrackByte(int idx) override;
	EmuTime getNextSector(EmuTime::param time, RawTrack::Sector& sector) override;
	void flushTrack() override;
	bool diskChanged() override;
	bool peekDiskChanged() const override;
	bool isDummyDrive() const override;
	void applyWd2793ReadTrackQuirk() override;
	void invalidateWd2793ReadTrackQuirk() override;

	bool isDiskInserted(DriveNum num) const;
	bool diskChanged(DriveNum num);
	bool peekDiskChanged(DriveNum num) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	DummyDrive dummyDrive;
	DiskDrive* drive[5];
	DriveNum selected;
	bool motor;
	bool side;
};

} // namespace openmsx

#endif
