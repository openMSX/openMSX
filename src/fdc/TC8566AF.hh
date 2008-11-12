// $Id$

#ifndef TC8566AF_HH
#define TC8566AF_HH

#include "Clock.hh"
#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class DiskDrive;

class TC8566AF : private noncopyable
{
public:
	TC8566AF(DiskDrive* drive[4], EmuTime::param time);

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
	byte peekDataPort() const;
	byte readDataPort(EmuTime::param time);
	byte peekStatus() const;
	byte readStatus(EmuTime::param time);
	byte executionPhasePeek() const;
	byte executionPhaseRead();
	byte resultsPhasePeek() const;
	byte resultsPhaseRead();
	void writeDataPort(byte value, EmuTime::param time);
	void idlePhaseWrite(byte value);
	void commandPhase1(byte value);
	void commandPhaseWrite(byte value, EmuTime::param time);
	void executionPhaseWrite(byte value);

	DiskDrive* drive[4];
	Clock<1000000> delayTime;

	Command command;
	Phase phase;
	int phaseStep;

	int sectorSize;
	int sectorOffset;
	//bool interrupt;

	byte sectorBuf[4096];

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
};

} // namespace openmsx

#endif
