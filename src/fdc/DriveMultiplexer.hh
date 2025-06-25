#ifndef DRIVEMULTIPLEXER_HH
#define DRIVEMULTIPLEXER_HH

#include "DiskDrive.hh"

#include "stl.hh"

#include <cstdint>
#include <memory>
#include <span>

namespace openmsx {

/**
 * This class connects to a FDC as a normal DiskDrive and delegates all
 * requests to one of four other DiskDrives.
 */
class DriveMultiplexer final : public DiskDrive
{
public:
	enum class Drive : uint8_t {
		A, B, C, D, NONE,
		NUM
	};

	// Multiplexer interface
	explicit DriveMultiplexer(std::span<std::unique_ptr<DiskDrive>, 4> drv);

	void selectDrive(Drive num, EmuTime time);
	[[nodiscard]] Drive getSelectedDrive() const { return selected; }

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

	[[nodiscard]] bool isDiskInserted(Drive num) const;
	bool diskChanged(Drive num);
	[[nodiscard]] bool peekDiskChanged(Drive num) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	DummyDrive dummyDrive;
	array_with_enum_index<Drive, DiskDrive*> drive;
	Drive selected = Drive::NONE;
	bool motor = false;
	bool side = false;
};

} // namespace openmsx

#endif
