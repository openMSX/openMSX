#ifndef WD2793_HH
#define WD2793_HH

#include "RawTrack.hh"

#include "DynamicClock.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"

#include "CRC16.hh"

namespace openmsx {

class Scheduler;
class DiskDrive;
class MSXCliComm;

class WD2793 final : public Schedulable
{
public:
	WD2793(Scheduler& scheduler, DiskDrive& drive, MSXCliComm& cliComm,
	       EmuTime time, bool isWD1770);

	void reset(EmuTime time);

	[[nodiscard]] uint8_t getStatusReg(EmuTime time);
	[[nodiscard]] uint8_t getTrackReg (EmuTime time) const;
	[[nodiscard]] uint8_t getSectorReg(EmuTime time) const;
	[[nodiscard]] uint8_t getDataReg  (EmuTime time);

	[[nodiscard]] uint8_t peekStatusReg(EmuTime time) const;
	[[nodiscard]] uint8_t peekTrackReg (EmuTime time) const;
	[[nodiscard]] uint8_t peekSectorReg(EmuTime time) const;
	[[nodiscard]] uint8_t peekDataReg  (EmuTime time) const;

	void setCommandReg(uint8_t value, EmuTime time);
	void setTrackReg  (uint8_t value, EmuTime time);
	void setSectorReg (uint8_t value, EmuTime time);
	void setDataReg   (uint8_t value, EmuTime time);

	[[nodiscard]] bool getIRQ (EmuTime time) const;
	[[nodiscard]] bool getDTRQ(EmuTime time) const;

	[[nodiscard]] bool peekIRQ (EmuTime time) const;
	[[nodiscard]] bool peekDTRQ(EmuTime time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum class FSM : uint8_t {
		NONE,
		SEEK,
		TYPE2_LOADED,
		TYPE2_NOT_FOUND,
		TYPE2_ROTATED,
		CHECK_WRITE,
		PRE_WRITE_SECTOR,
		WRITE_SECTOR,
		POST_WRITE_SECTOR,
		TYPE3_LOADED,
		TYPE3_ROTATED,
		WRITE_TRACK,
		READ_TRACK,
		IDX_IRQ
	};

private:
	void executeUntil(EmuTime time) override;

	void startType1Cmd(EmuTime time);

	void seek(EmuTime time);
	void step(EmuTime time);
	void seekNext(EmuTime time);
	void endType1Cmd(EmuTime time);

	void startType2Cmd   (EmuTime time);
	void type2Loaded     (EmuTime time);
	void type2Search     (EmuTime time);
	void type2NotFound   (EmuTime time);
	void type2Rotated    (EmuTime time);
	void startReadSector (EmuTime time);
	void startWriteSector(EmuTime time);
	void checkStartWrite (EmuTime time);
	void preWriteSector  (EmuTime time);
	void writeSectorData (EmuTime time);
	void postWriteSector (EmuTime time);

	void startType3Cmd   (EmuTime time);
	void type3Loaded     (EmuTime time);
	void type3Rotated    (EmuTime time);
	void readAddressCmd  (EmuTime time);
	void readTrackCmd    (EmuTime time);
	void startWriteTrack (EmuTime time);
	void writeTrackData  (EmuTime time);

	void startType4Cmd(EmuTime time);

	void endCmd(EmuTime time);

	void setDrqRate(unsigned trackLength);
	[[nodiscard]] bool isReady() const;

	void schedule(FSM state, EmuTime time);

private:
	DiskDrive& drive;
	MSXCliComm& cliComm;

	// DRQ is high iff current time is past this time.
	//  This clock ticks at the 'byte-rate' of the current track,
	//  typically '6250 bytes/rotation * 5 rotations/second'.
	DynamicClock drqTime{EmuTime::infinity()};

	// INTRQ is high iff current time is past this time.
	EmuTime irqTime = EmuTime::infinity();

	EmuTime pulse5 = EmuTime::infinity(); // time at which the 5th index pulse will be received

	// HLD/HLT timing
	// 1) now < hldTime   (typically: hldTime == EmuTime::infinity)
	//     ==> HLD=false, WD2793 did not request head to be loaded
	// 2) hldTime <= now < hldTime + LOAD
	//     ==> HLD=true, HLT=false, head loading is in progress
	// 3) hldTime + LOAD <= now < hldTime + LOAD + IDLE
	//     ==> HLD=true, HLT=true, head is loaded
	// 4) hldTime + LOAD + IDLE <= now
	//     ==> HLD=false, idle for 15 revolutions HLD made deactive again
	// Note: is all MSX machines I've checked so far, WD2793 pins
	//  - HLD (pin 28) is unconnected
	//  - HLT (pin 40) is connected to +5V
	// This means there is no head-load delay, or IOW as soon as the WD2793
	// requests to load the disk head, the drive responds with "ok, done".
	// So 'LOAD' is zero, and phase 2) doesn't exist. Therefor the current
	// implementation mostly ignores 'LOAD'.
	EmuTime hldTime = EmuTime::infinity(); // HLD=false

	RawTrack::Sector sectorInfo;
	int dataCurrent   = 0; // which byte in track is next to be read/write
	int dataAvailable = 0; // how many bytes left to read/write

	CRC16 crc;

	FSM fsmState;
	uint8_t statusReg;
	uint8_t commandReg = 0;
	uint8_t sectorReg;
	uint8_t trackReg;
	uint8_t dataReg;
	uint8_t dataOutReg = 0;

	bool directionIn;
	bool immediateIRQ;
	bool lastWasA1 = false;
	bool dataRegWritten = false;
	bool lastWasCRC = false;

	const bool isWD1770;
};
SERIALIZE_CLASS_VERSION(WD2793, 12);

} // namespace openmsx

#endif
