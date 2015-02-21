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
	byte getTrackReg (EmuTime::param time);
	byte getSectorReg(EmuTime::param time);
	byte getDataReg  (EmuTime::param time);

	byte peekStatusReg(EmuTime::param time) const;
	byte peekTrackReg (EmuTime::param time) const;
	byte peekSectorReg(EmuTime::param time) const;
	byte peekDataReg  (EmuTime::param time) const;

	void setCommandReg(byte value, EmuTime::param time);
	void setTrackReg  (byte value, EmuTime::param time);
	void setSectorReg (byte value, EmuTime::param time);
	void setDataReg   (byte value, EmuTime::param time);

	bool getIRQ (EmuTime::param time);
	bool getDTRQ(EmuTime::param time);

	bool peekIRQ (EmuTime::param time) const;
	bool peekDTRQ(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum FSMState {
		FSM_NONE,
		FSM_SEEK,
		FSM_TYPE2_WAIT_LOAD,
		FSM_TYPE2_LOADED,
		FSM_TYPE2_NOT_FOUND,
		FSM_TYPE2_ROTATED,
		FSM_CHECK_WRITE,
		FSM_WRITE_SECTOR,
		FSM_TYPE3_WAIT_LOAD,
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
	void endType1Cmd();

	void startType2Cmd   (EmuTime::param time);
	void type2WaitLoad   (EmuTime::param time);
	void type2Loaded     (EmuTime::param time);
	void type2Search     (EmuTime::param time);
	void type2NotFound   ();
	void type2Rotated    (EmuTime::param time);
	void startReadSector (EmuTime::param time);
	void startWriteSector(EmuTime::param time);
	void checkStartWrite (EmuTime::param time);
	void doneWriteSector();

	void startType3Cmd (EmuTime::param time);
	void type3WaitLoad (EmuTime::param time);
	void type3Loaded   (EmuTime::param time);
	void type3Rotated  (EmuTime::param time);
	void readAddressCmd(EmuTime::param time);
	void readTrackCmd  (EmuTime::param time);
	void writeTrackCmd (EmuTime::param time);
	void doneWriteTrack();

	void startType4Cmd(EmuTime::param time);

	void endCmd();

	void setDrqRate();
	bool isReady() const;

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

	RawTrack trackData;
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

	bool directionIn;
	bool immediateIRQ;
	bool lastWasA1;

	const bool isWD1770;
};
SERIALIZE_CLASS_VERSION(WD2793, 8);

} // namespace openmsx

#endif
