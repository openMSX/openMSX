#ifndef TC8566AF_HH
#define TC8566AF_HH

#include "DynamicClock.hh"
#include "CRC16.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"

namespace openmsx {

class Scheduler;
class DiskDrive;
class CliComm;

class TC8566AF final : public Schedulable
{
public:
	TC8566AF(Scheduler& scheduler, DiskDrive* drv[4], CliComm& cliComm,
	         EmuTime::param time);

	void reset(EmuTime::param time);
	[[nodiscard]] byte peekDataPort(EmuTime::param time) const;
	byte readDataPort(EmuTime::param time);
	[[nodiscard]] byte peekStatus() const;
	byte readStatus(EmuTime::param time);
	void writeControlReg0(byte value, EmuTime::param time);
	void writeControlReg1(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);
	bool diskChanged(unsigned driveNum);
	[[nodiscard]] bool peekDiskChanged(unsigned driveNum) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum Command {
		CMD_UNKNOWN,
		CMD_READ_DATA,
		CMD_WRITE_DATA,
		CMD_WRITE_DELETED_DATA,
		CMD_READ_DELETED_DATA,
		CMD_READ_DIAGNOSTIC,
		CMD_READ_ID,
		CMD_FORMAT,
		CMD_SCAN_EQUAL,
		CMD_SCAN_LOW_OR_EQUAL,
		CMD_SCAN_HIGH_OR_EQUAL,
		CMD_SEEK,
		CMD_RECALIBRATE,
		CMD_SENSE_INTERRUPT_STATUS,
		CMD_SPECIFY,
		CMD_SENSE_DEVICE_STATUS,
	};
	enum Phase {
		PHASE_IDLE,
		PHASE_COMMAND,
		PHASE_DATATRANSFER,
		PHASE_RESULT,
	};
	enum SeekState {
		SEEK_IDLE,
		SEEK_SEEK,
		SEEK_RECALIBRATE
	};

private:
	// Schedulable
	void executeUntil(EmuTime::param time) override;

	[[nodiscard]] byte executionPhasePeek(EmuTime::param time) const;
	byte executionPhaseRead(EmuTime::param time);
	[[nodiscard]] byte resultsPhasePeek() const;
	byte resultsPhaseRead(EmuTime::param time);
	void idlePhaseWrite(byte value, EmuTime::param time);
	void commandPhase1(byte value);
	void commandPhaseWrite(byte value, EmuTime::param time);
	void doSeek(int n);
	void executionPhaseWrite(byte value, EmuTime::param time);
	void resultPhase();
	void endCommand(EmuTime::param time);

	[[nodiscard]] bool isHeadLoaded(EmuTime::param time) const;
	[[nodiscard]] EmuDuration getHeadLoadDelay() const;
	[[nodiscard]] EmuDuration getHeadUnloadDelay() const;
	[[nodiscard]] EmuDuration getSeekDelay() const;

	[[nodiscard]] EmuTime locateSector(EmuTime::param time);
	void startReadWriteSector(EmuTime::param time);
	void writeSector();
	void initTrackHeader(EmuTime::param time);
	void formatSector();
	void setDrqRate(unsigned trackLength);

private:
	CliComm& cliComm;
	DiskDrive* drive[4];
	DynamicClock delayTime;
	EmuTime headUnloadTime; // Before this time head is loaded, after
	                        // this time it's unloaded. Set to zero/infinity
	                        // to force a (un)loaded head.

	Command command;
	Phase phase;
	int phaseStep;

	//bool interrupt;

	int dataAvailable;
	int dataCurrent;
	CRC16 crc;

	byte driveSelect;
	byte mainStatus;
	byte status0;
	byte status1;
	byte status2;
	byte status3;
	byte commandCode;

	byte cylinderNumber;
	byte headNumber;
	byte sectorNumber;
	byte number;
	byte endOfTrack;
	byte sectorsPerCylinder;
	byte fillerByte;
	byte gapLength;
	byte specifyData[2]; // filled in by SPECIFY command

	struct SeekInfo {
		EmuTime time = EmuTime::zero();
		byte currentTrack = 0;
		byte seekValue = 0;
		SeekState state = SEEK_IDLE;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	} seekInfo[4];
};
SERIALIZE_CLASS_VERSION(TC8566AF, 7);

} // namespace openmsx

#endif
