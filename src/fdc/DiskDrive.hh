#ifndef DISKDRIVE_HH
#define DISKDRIVE_HH

#include "EmuTime.hh"
#include "RawTrack.hh"

#include <cstdint>

namespace openmsx {

/**
 * This (abstract) class defines the DiskDrive interface
 */
class DiskDrive
{
public:
	enum class TrackMode : uint8_t {
		NORMAL, YAMAHA_FD_03,
	};

public:
	static constexpr unsigned ROTATIONS_PER_SECOND = 5; // 300rpm

	virtual ~DiskDrive() = default;

	/** Is drive ready?
	 */
	[[nodiscard]] virtual bool isDiskInserted() const = 0;

	/** Is disk write protected?
	 */
	[[nodiscard]] virtual bool isWriteProtected() const = 0;

	/** Is disk double sided?
	 */
	[[nodiscard]] virtual bool isDoubleSided() = 0;

	/** Head above track 0
	 */
	[[nodiscard]] virtual bool isTrack00() const = 0;

	/** Side select.
	 * @param side false = side 0,
	 *             true  = side 1.
	 */
	virtual void setSide(bool side) = 0;

	/* Returns the previously selected side.
	 */
	[[nodiscard]] virtual bool getSide() const = 0;

	/** Step head
	 * @param direction false = out,
	 *                  true  = in.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void step(bool direction, EmuTime time) = 0;

	/** Set motor on/off
	 * @param status false = off,
	 *               true  = on.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setMotor(bool status, EmuTime time) = 0;

	/** Returns the previously set motor status.
	 */
	[[nodiscard]] virtual bool getMotor() const = 0;

	/** Gets the state of the index pulse.
	 * @param time The moment in emulated time to get the pulse state for.
	 */
	[[nodiscard]] virtual bool indexPulse(EmuTime time) = 0;

	/** Return the time till the start of the next index pulse
	 * When there is no disk in the drive or when the disk is not spinning,
	 * this function returns the current time.
	 * @param time The current time
	 * @param count Number of required index pulses.
	 */
	[[nodiscard]] virtual EmuTime getTimeTillIndexPulse(EmuTime time, int count = 1) = 0;

	[[nodiscard]] virtual unsigned getTrackLength() = 0;
	virtual void writeTrackByte(int idx, uint8_t val, bool addIdam = false) = 0;
	[[nodiscard]] virtual uint8_t readTrackByte(int idx) = 0;
	virtual EmuTime getNextSector(EmuTime time, RawTrack::Sector& sector) = 0;
	virtual void flushTrack() = 0;

	/** Is disk changed?
	 */
	virtual bool diskChanged() = 0;           // read and reset
	[[nodiscard]] virtual bool peekDiskChanged() const = 0; // read without reset

	/** Is there a dummy (unconnected) drive?
	 */
	[[nodiscard]] virtual bool isDummyDrive() const = 0;

	/** See RawTrack::applyWd2793ReadTrackQuirk() */
	virtual void applyWd2793ReadTrackQuirk() = 0;
	virtual void invalidateWd2793ReadTrackQuirk() = 0;
};


/**
 * This class implements a not connected disk drive.
 */
class DummyDrive final : public DiskDrive
{
public:
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
};

} // namespace openmsx

#endif
