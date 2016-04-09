#ifndef DISKDRIVE_HH
#define DISKDRIVE_HH

#include "EmuTime.hh"
#include "RawTrack.hh"

namespace openmsx {

/**
 * This (abstract) class defines the DiskDrive interface
 */
class DiskDrive
{
public:
	static const unsigned ROTATIONS_PER_SECOND = 5; // 300rpm

	virtual ~DiskDrive();

	/** Is drive ready?
	 */
	virtual bool isDiskInserted() const = 0;

	/** Is disk write protected?
	 */
	virtual bool isWriteProtected() const = 0;

	/** Is disk double sided?
	 */
	virtual bool isDoubleSided() const = 0;

	/** Head above track 0
	 */
	virtual bool isTrack00() const = 0;

	/** Side select.
	 * @param side false = side 0,
	 *             true  = side 1.
	 */
	virtual void setSide(bool side) = 0;

	/** Step head
	 * @param direction false = out,
	 *                  true  = in.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void step(bool direction, EmuTime::param time) = 0;

	/** Set motor on/off
	 * @param status false = off,
	 *               true  = on.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setMotor(bool status, EmuTime::param time) = 0;

	/** Gets the state of the index pulse.
	 * @param time The moment in emulated time to get the pulse state for.
	 */
	virtual bool indexPulse(EmuTime::param time) = 0;

	/** Return the time till the start of the next index pulse
	 * When there is no disk in the drive or when the disk is not spinning,
	 * this function returns the current time.
	 * @param time The current time
	 * @param count Number of required index pulses.
	 */
	virtual EmuTime getTimeTillIndexPulse(EmuTime::param time, int count = 1) = 0;

	/** Set head loaded status.
	 * @param status false = not loaded,
	 *               true  = loaded.
	 * @param time The moment in emulated time this action takes place.
	 */
	virtual void setHeadLoaded(bool status, EmuTime::param time) = 0;

	/** Is head loaded?
	 */
	virtual bool headLoaded(EmuTime::param time) = 0;

	virtual void writeTrack(const RawTrack& track) = 0;
	virtual void readTrack (      RawTrack& track) = 0;
	virtual EmuTime getNextSector(EmuTime::param time, RawTrack& track,
	                              RawTrack::Sector& sector) = 0;

	/** Is disk changed?
	 */
	virtual bool diskChanged() = 0;
	virtual bool peekDiskChanged() const = 0;

	/** Is there a dummy (unconncted) drive?
	 */
	virtual bool isDummyDrive() const = 0;
};


/**
 * This class implements a not connected disk drive.
 */
class DummyDrive final : public DiskDrive
{
public:
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
};

} // namespace openmsx

#endif
