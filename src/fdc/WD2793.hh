#ifndef WD2793_HH
#define WD2793_HH

#include "RawTrack.hh"
#include "DynamicClock.hh"
#include "Schedulable.hh"
#include "CRC16.hh"
#include "serialize_meta.hh"

namespace openmsx {

class Scheduler;
class DiskDrive;
class CliComm;

class WD2793 final : public Schedulable
{
public:
	WD2793(Scheduler& scheduler, DiskDrive& drive, CliComm& cliComm,
	       EmuTime::param time, bool isWD1770);

	void reset(EmuTime::param time);

	byte getStatusReg(EmuTime::param time);
	byte getTrackReg (EmuTime::param time) const;
	byte getSectorReg(EmuTime::param time) const;
	byte getDataReg  (EmuTime::param time);

	[[nodiscard]] byte peekStatusReg(EmuTime::param time) const;
	[[nodiscard]] byte peekTrackReg (EmuTime::param time) const;
	[[nodiscard]] byte peekSectorReg(EmuTime::param time) const;
	[[nodiscard]] byte peekDataReg  (EmuTime::param time) const;

	void setCommandReg(byte value, EmuTime::param time);
	void setTrackReg  (byte value, EmuTime::param time);
	void setSectorReg (byte value, EmuTime::param time);
	void setDataReg   (byte value, EmuTime::param time);

	bool getIRQ (EmuTime::param time) const;
	bool getDTRQ(EmuTime::param time) const;

	[[nodiscard]] bool peekIRQ (EmuTime::param time) const;
	[[nodiscard]] bool peekDTRQ(EmuTime::param time) const;

	void serialize(Archive auto& ar, unsigned version);

	// public for serialize
	enum FSMState {
		FSM_NONE,
		FSM_SEEK,
		FSM_TYPE2_LOADED,
		FSM_TYPE2_NOT_FOUND,
		FSM_TYPE2_ROTATED,
		FSM_CHECK_WRITE,
		FSM_PRE_WRITE_SECTOR,
		FSM_WRITE_SECTOR,
		FSM_POST_WRITE_SECTOR,
		FSM_TYPE3_LOADED,
		FSM_TYPE3_ROTATED,
		FSM_WRITE_TRACK,
		FSM_READ_TRACK,
		FSM_IDX_IRQ
	};

private:
	void executeUntil(EmuTime::param time) override;

	void startType1Cmd(EmuTime::param time);

	void seek(EmuTime::param time);
	void step(EmuTime::param time);
	void seekNext(EmuTime::param time);
	void endType1Cmd(EmuTime::param time);

	void startType2Cmd   (EmuTime::param time);
	void type2Loaded     (EmuTime::param time);
	void type2Search     (EmuTime::param time);
	void type2NotFound   (EmuTime::param time);
	void type2Rotated    (EmuTime::param time);
	void startReadSector (EmuTime::param time);
	void startWriteSector(EmuTime::param time);
	void checkStartWrite (EmuTime::param time);
	void preWriteSector  (EmuTime::param time);
	void writeSectorData (EmuTime::param time);
	void postWriteSector (EmuTime::param time);

	void startType3Cmd   (EmuTime::param time);
	void type3Loaded     (EmuTime::param time);
	void type3Rotated    (EmuTime::param time);
	void readAddressCmd  (EmuTime::param time);
	void readTrackCmd    (EmuTime::param time);
	void startWriteTrack (EmuTime::param time);
	void writeTrackData  (EmuTime::param time);

	void startType4Cmd(EmuTime::param time);

	void endCmd(EmuTime::param time);

	void setDrqRate(unsigned trackLength);
	[[nodiscard]] bool isReady() const;

	void schedule(FSMState state, EmuTime::param time);

private:
	DiskDrive& drive;
	CliComm& cliComm;

	// DRQ is high iff current time is past this time.
	//  This clock ticks at the 'byte-rate' of the current track,
	//  typically '6250 bytes/rotation * 5 rotations/second'.
	DynamicClock drqTime;

	// INTRQ is high iff current time is past this time.
	EmuTime irqTime;

	EmuTime pulse5; // time at which the 5th index pulse will be received

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
	EmuTime hldTime;

	RawTrack::Sector sectorInfo;
	int dataCurrent;   // which byte in track is next to be read/write
	int dataAvailable; // how many bytes left to read/write

	CRC16 crc;

	FSMState fsmState;
	byte statusReg;
	byte commandReg;
	byte sectorReg;
	byte trackReg;
	byte dataReg;
	byte dataOutReg;

	bool directionIn;
	bool immediateIRQ;
	bool lastWasA1;
	bool dataRegWritten;
	bool lastWasCRC;

	const bool isWD1770;
};
SERIALIZE_CLASS_VERSION(WD2793, 12);

} // namespace openmsx

#endif
