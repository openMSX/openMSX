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
	TC8566AF(DiskDrive* drive[4], const EmuTime& time);

	void reset(const EmuTime& time);
	byte readReg(int reg, const EmuTime& time);
	byte peekReg(int reg, const EmuTime& time) const;
	void writeReg(int reg, byte data, const EmuTime& time);
	bool diskChanged(unsigned driveNum);
	bool peekDiskChanged(unsigned driveNum) const;

private:
	byte peekDataPort() const;
	byte readDataPort(const EmuTime& time);
	byte peekStatus() const;
	byte readStatus(const EmuTime& time);
	byte executionPhasePeek() const;
	byte executionPhaseRead();
	byte resultsPhasePeek() const;
	byte resultsPhaseRead();
	void writeDataPort(byte value, const EmuTime& time);
	void idlePhaseWrite(byte value);
	void commandPhase1(byte value, const EmuTime& time);
	void commandPhaseWrite(byte value, const EmuTime& time);
	void executionPhaseWrite(byte value);

	DiskDrive* drive[4]; 
	byte driveSelect;
	byte mainStatus;
	byte status0;
	byte status1;
	byte status2;
	byte status3;
	byte commandCode;

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
	} command;
	enum Phase {
		PHASE_IDLE,
		PHASE_COMMAND,
		PHASE_DATATRANSFER,
		PHASE_RESULT,
	} phase;
	int phaseStep;

	byte cylinderNumber;
	byte headNumber;
	byte sectorNumber;
	byte number;
	byte currentTrack;
	byte sectorsPerCylinder;
	byte fillerByte;

	int sectorSize;
	int sectorOffset;
	//bool interrupt;

	byte sectorBuf[4096];
	Clock<1000000> delayTime;
};

} // namespace openmsx

#endif
