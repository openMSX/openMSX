#ifndef TC8566AF_HH
#define TC8566AF_HH

#include "DynamicClock.hh"
#include "RawTrack.hh"
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
	TC8566AF(Scheduler& scheduler, DiskDrive* drive[4], CliComm& cliComm,
	         EmuTime::param time);

	void reset(EmuTime::param time);
	byte readReg(int reg, EmuTime::param time);
	byte peekReg(int reg, EmuTime::param time) const;
	void writeReg(int reg, byte data, EmuTime::param time);
	bool diskChanged(unsigned driveNum);
	bool peekDiskChanged(unsigned driveNum) const;

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

private:
	// Schedulable
	void executeUntil(EmuTime::param time) override;

	byte peekDataPort(EmuTime::param time) const;
	byte readDataPort(EmuTime::param time);
	byte peekStatus() const;
	byte readStatus(EmuTime::param time);
	byte executionPhasePeek(EmuTime::param time) const;
	byte executionPhaseRead(EmuTime::param time);
	byte resultsPhasePeek() const;
	byte resultsPhaseRead(EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);
	void idlePhaseWrite(byte value, EmuTime::param time);
	void commandPhase1(byte value);
	void commandPhaseWrite(byte value, EmuTime::param time);
	void doSeek(EmuTime::param time);
	void executionPhaseWrite(byte value, EmuTime::param time);
	void resultPhase();
	void endCommand(EmuTime::param time);

	bool isHeadLoaded(EmuTime::param time) const;
	EmuDuration getHeadLoadDelay() const;
	EmuDuration getHeadUnloadDelay() const;
	EmuDuration getSeekDelay() const;

	EmuTime locateSector(EmuTime::param time);
	void writeSector();
	void initTrackHeader(EmuTime::param time);
	void formatSector();
	void setDrqRate();

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

	RawTrack trackData;
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
	byte currentTrack;
	byte sectorsPerCylinder;
	byte fillerByte;
	byte gapLength;
	byte specifyData[2]; // filled in by SPECIFY command
	byte seekValue;
};
SERIALIZE_CLASS_VERSION(TC8566AF, 4);

} // namespace openmsx

#endif
