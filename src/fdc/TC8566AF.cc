// $Id$

/*
 * Based on code from NLMSX written by Frits Hilderink
 *        and       blueMSX written by Daniel Vik
 */

#include "TC8566AF.hh"
#include "DiskDrive.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

static const byte STM_DB0 = 0x01;
static const byte STM_DB1 = 0x02;
static const byte STM_DB2 = 0x04;
static const byte STM_DB3 = 0x08;
static const byte STM_CB  = 0x10;
static const byte STM_NDM = 0x20;
static const byte STM_DIO = 0x40;
static const byte STM_RQM = 0x80;

static const byte ST0_DS0 = 0x01;
static const byte ST0_DS1 = 0x02;
static const byte ST0_HD  = 0x04;
static const byte ST0_NR  = 0x08;
static const byte ST0_EC  = 0x10;
static const byte ST0_SE  = 0x20;
static const byte ST0_IC0 = 0x40;
static const byte ST0_IC1 = 0x80;

static const byte ST1_MA  = 0x01;
static const byte ST1_NW  = 0x02;
static const byte ST1_ND  = 0x04;
static const byte ST1_OR  = 0x10;
static const byte ST1_DE  = 0x20;
static const byte ST1_EN  = 0x80;

static const byte ST2_MD  = 0x01;
static const byte ST2_BC  = 0x02;
static const byte ST2_SN  = 0x04;
static const byte ST2_SH  = 0x08;
static const byte ST2_NC  = 0x10;
static const byte ST2_DD  = 0x20;
static const byte ST2_CM  = 0x40;

static const byte ST3_DS0 = 0x01;
static const byte ST3_DS1 = 0x02;
static const byte ST3_HD  = 0x04;
static const byte ST3_2S  = 0x08;
static const byte ST3_TK0 = 0x10;
static const byte ST3_RDY = 0x20;
static const byte ST3_WP  = 0x40;
static const byte ST3_FLT = 0x80;


TC8566AF::TC8566AF(DiskDrive* drv[4], const EmuTime& time)
	: delayTime(EmuTime::zero)
{
	// avoid UMR (on savestate)
	memset(sectorBuf, 0, sizeof(sectorBuf));

	drive[0] = drv[0];
	drive[1] = drv[1];
	drive[2] = drv[2];
	drive[3] = drv[3];
	reset(time);
}

void TC8566AF::reset(const EmuTime& time)
{
	drive[0]->setMotor(false, time);
	drive[1]->setMotor(false, time);
	drive[2]->setMotor(false, time);
	drive[3]->setMotor(false, time);
	//enableIntDma = 0;
	//notReset = 1;
	driveSelect = 0;

	status0 = 0;
	status1 = 0;
	status2 = 0;
	status3 = 0;
	commandCode = 0;
	command = CMD_UNKNOWN;
	phase = PHASE_IDLE;
	phaseStep = 0;
	cylinderNumber = 0;
	headNumber = 0;
	sectorNumber = 0;
	number = 0;
	currentTrack = 0;
	sectorsPerCylinder = 0;
	fillerByte = 0;
	sectorSize = 0;
	sectorOffset = 0;

	mainStatus = STM_RQM;
	//interrupt = false;
}

byte TC8566AF::peekReg(int reg, const EmuTime& /*time*/) const
{
	switch (reg) {
	case 4: // Main Status Register
		return peekStatus();
	case 5: // data port
		return peekDataPort();
	}
	return 0xff;
}

byte TC8566AF::readReg(int reg, const EmuTime& time)
{
	switch (reg) {
	case 4: // Main Status Register
		return readStatus(time);
	case 5: // data port
		return readDataPort(time);
	}
	return 0xff;
}

byte TC8566AF::peekStatus() const
{
	bool nonDMAMode = true; // TODO
	bool dma = nonDMAMode && (phase == PHASE_DATATRANSFER);
	return mainStatus | (dma ? STM_NDM : 0);
}

byte TC8566AF::readStatus(const EmuTime& time)
{
	if (delayTime.before(time)) {
		mainStatus |= STM_RQM;
	}
	return peekStatus();
}

byte TC8566AF::peekDataPort() const
{
	switch (phase) {
	case PHASE_DATATRANSFER:
		return executionPhasePeek();
	case PHASE_RESULT:
		return resultsPhasePeek();
	default:
		return 0xff;
	}
}

byte TC8566AF::readDataPort(const EmuTime& time)
{
	//interrupt = false;
	switch (phase) {
	case PHASE_DATATRANSFER: {
		byte result = executionPhaseRead();
		delayTime.reset(time);
		delayTime += 15;  // TODO 15 correct?
		mainStatus &= ~STM_RQM;
		return result;
	}
	case PHASE_RESULT:
		return resultsPhaseRead();
	default:
		return 0xff;
	}
}

byte TC8566AF::executionPhasePeek() const
{
	switch (command) {
	case CMD_READ_DATA:
		if (sectorOffset < sectorSize) {
			return sectorBuf[sectorOffset];
		}
		// fall-through
	default:
		return 0xff;
	}
}

byte TC8566AF::executionPhaseRead()
{
	switch (command) {
	case CMD_READ_DATA:
		if (sectorOffset < sectorSize) {
			byte value = sectorBuf[sectorOffset++];
			if (sectorOffset == sectorSize) {
				phase = PHASE_RESULT;
				phaseStep = 0;
				//interrupt = true;
			}
			return value;
		}
		// fall-through
	default:
		return 0xff;
	}
}

byte TC8566AF::resultsPhasePeek() const
{
	switch (command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		switch (phaseStep) {
		case 0:
			return status0;
		case 1:
			return status1;
		case 2:
			return status2;
		case 3:
			return cylinderNumber;
		case 4:
			return headNumber;
		case 5:
			return sectorNumber;
		case 6:
			return number;
		}
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		switch (phaseStep) {
		case 0:
			return status0;
		case 1:
			return currentTrack;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (phaseStep) {
		case 0:
			return status3;
		}
		break;
	default:
		// nothing
		break;
	}
	return 0xff;
}

byte TC8566AF::resultsPhaseRead()
{
	byte result = resultsPhasePeek();
	switch (command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		switch (phaseStep++) {
		case 6:
			phase       = PHASE_IDLE;
			mainStatus &= ~(STM_CB | STM_DIO);
			break;
		}
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		switch (phaseStep++) {
		case 1:
			phase       = PHASE_IDLE;
			mainStatus &= ~(STM_CB | STM_DIO);
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (phaseStep++) {
		case 0:
			phase       = PHASE_IDLE;
			mainStatus &= ~(STM_CB | STM_DIO);
			break;
		}
		break;
	default:
		// nothing
		break;
	}
	return result;
}

void TC8566AF::writeReg(int reg, byte data, const EmuTime& time)
{
	switch (reg) {
	case 2: // control register 0
		drive[3]->setMotor(data & 0x80, time);
		drive[2]->setMotor(data & 0x40, time);
		drive[1]->setMotor(data & 0x20, time);
		drive[0]->setMotor(data & 0x10, time);
		//enableIntDma = data & 0x08;
		//notReset     = data & 0x04;
		driveSelect = data & 0x03;
		break;

	//case 3: // control register 1
	//	controlReg1 = data;
	//	break;

	case 5: // data port
		writeDataPort(data, time);
		break;
	}
}

void TC8566AF::writeDataPort(byte value, const EmuTime& time)
{
	switch (phase) {
	case PHASE_IDLE:
		idlePhaseWrite(value);
		break;

	case PHASE_COMMAND:
		commandPhaseWrite(value, time);
		break;

	case PHASE_DATATRANSFER:
		executionPhaseWrite(value);
		delayTime.reset(time);
		delayTime += 15;
		mainStatus &= ~STM_RQM;
		break;
	default:
		//nothing
		break;
	}
}

void TC8566AF::idlePhaseWrite(byte value)
{
	command = CMD_UNKNOWN;
	commandCode = value;
	if ((commandCode & 0x1f) == 0x06) command = CMD_READ_DATA;
	if ((commandCode & 0x3f) == 0x05) command = CMD_WRITE_DATA;
	if ((commandCode & 0x3f) == 0x09) command = CMD_WRITE_DELETED_DATA;
	if ((commandCode & 0x1f) == 0x0c) command = CMD_READ_DELETED_DATA;
	if ((commandCode & 0xbf) == 0x02) command = CMD_READ_DIAGNOSTIC;
	if ((commandCode & 0xbf) == 0x0a) command = CMD_READ_ID;
	if ((commandCode & 0xbf) == 0x0d) command = CMD_FORMAT;
	if ((commandCode & 0x1f) == 0x11) command = CMD_SCAN_EQUAL;
	if ((commandCode & 0x1f) == 0x19) command = CMD_SCAN_LOW_OR_EQUAL;
	if ((commandCode & 0x1f) == 0x1d) command = CMD_SCAN_HIGH_OR_EQUAL;
	if ((commandCode & 0xff) == 0x0f) command = CMD_SEEK;
	if ((commandCode & 0xff) == 0x07) command = CMD_RECALIBRATE;
	if ((commandCode & 0xff) == 0x08) command = CMD_SENSE_INTERRUPT_STATUS;
	if ((commandCode & 0xff) == 0x03) command = CMD_SPECIFY;
	if ((commandCode & 0xff) == 0x04) command = CMD_SENSE_DEVICE_STATUS;

	phase       = PHASE_COMMAND;
	phaseStep   = 0;
	mainStatus |= STM_CB;

	switch (command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		status0 &= ~(ST0_IC0 | ST0_IC1);
		status1 &= ~(ST1_ND | ST1_NW);
		//MT  = value & 0x80;
		//MFM = value & 0x40;
		//SK  = value & 0x20;
		break;

	case CMD_RECALIBRATE:
		status0 &= ~ST0_SE;
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		mainStatus |= STM_DIO;
		phase       = PHASE_RESULT;
		//interrupt   = true;
		break;

	case CMD_SEEK:
	case CMD_SPECIFY:
	case CMD_SENSE_DEVICE_STATUS:
		break;

	default:
		mainStatus &= ~STM_CB;
		phase       = PHASE_IDLE;
		//interrupt   = true;
	}
}

void TC8566AF::commandPhase1(byte value)
{
	drive[driveSelect]->setSide(value & 0x04);
	status0 &= ~(ST0_DS0 | ST0_DS1 | ST0_IC0 | ST0_IC1);
	status0 |= //(drive[driveSelect]->isReady() ? 0 : ST0_DS0) |
	           (value & (ST0_DS0 | ST0_DS1)) |
	           (drive[driveSelect]->dummyDrive() ? ST0_IC1 : 0);
	status3  = (value & (ST3_DS0 | ST3_DS1)) |
	           (drive[driveSelect]->isTrack00()        ? ST3_TK0 : 0) |
	           (drive[driveSelect]->isDoubleSided()    ? ST3_HD  : 0) |
	           (drive[driveSelect]->isWriteProtected() ? ST3_WP  : 0) |
	           (drive[driveSelect]->isReady()          ? ST3_RDY : 0);
}

void TC8566AF::commandPhaseWrite(byte value, const EmuTime& time)
{
	switch (command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			break;
		case 1:
			cylinderNumber = value;
			break;
		case 2:
			headNumber = value;
			break;
		case 3:
			sectorNumber = value;
			break;
		case 4:
			number = value;
			//sectorSize = diskGetSectorSize(driveSelect, side, currentTrack, 0);
			//sectorOffset = (number == 1 && (commandCode & 0xc0) == 0x40)
			//             ? 0
			//             : sectorSize; // FIXME
			//sectorSize = ((MT == 0) && (MFM == 1) && (number == 2)) ? 512 : 0;
			sectorSize = 512;
			sectorOffset = 0;
			break;
		case 5: // End Of Track
			break;
		case 6: // Gap Length
			break;
		case 7: // Data length
			if (command == CMD_READ_DATA) {
				try {
					byte onDiskTrack;
					byte onDiskSector;
					byte onDiskSide;
					int  onDiskSize;
					drive[driveSelect]->read(
						sectorNumber, sectorBuf,
						onDiskTrack, onDiskSector,
						onDiskSide,  onDiskSize);
				} catch (MSXException& e) {
					status0 |= ST0_IC0;
					status1 |= ST1_ND;
				}
				mainStatus |= STM_DIO;
			} else {
				mainStatus &= ~STM_DIO;
			}
			phase = PHASE_DATATRANSFER;
			phaseStep = 0;
			//interrupt = true;
			break;
		}
		break;

	case CMD_FORMAT:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			//sectorSize = diskGetSectorSize(driveSelect, side, currentTrack, 0);
			sectorSize = 512;
			break;
		case 1:
			number = value;
			break;
		case 2:
			sectorsPerCylinder = value;
			sectorNumber       = value;
			break;
		case 4:
			fillerByte   = value;
			sectorOffset = 0;
			mainStatus  &= ~STM_DIO;
			phase        = PHASE_DATATRANSFER;
			phaseStep    = 0;
			//interrupt    = true;
			break;
		}
		break;

	case CMD_SEEK:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			break;
		case 1:
			while (value > currentTrack) {
				drive[driveSelect]->step(true, time);
				currentTrack++;
			}
			while (value < currentTrack) {
				drive[driveSelect]->step(false, time);
				currentTrack--;
			}
			assert(currentTrack == value);
			status0     |= ST0_SE;
			mainStatus  &= ~STM_CB;
			phase        = PHASE_IDLE;
			//interrupt    = true;
			break;
		}
		break;

	case CMD_RECALIBRATE:
		switch (phaseStep++) {
		case 0: {
			commandPhase1(value);

			unsigned maxSteps = 255;
			while (!drive[driveSelect]->isTrack00() && maxSteps--) {
				drive[driveSelect]->step(false, time);
			}
			currentTrack = 0;
			status0     |= ST0_SE;
			mainStatus  &= ~STM_CB;
			phase        = PHASE_IDLE;
			//interrupt    = true;
			break;
		}
		}
		break;

	case CMD_SPECIFY:
		switch (phaseStep++) {
		case 1:
			mainStatus &= ~STM_CB;
			phase       = PHASE_IDLE;
			//interrupt   = true;
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);

			mainStatus |= STM_DIO;
			phase       = PHASE_RESULT;
			phaseStep   = 0;
			//interrupt   = true;
			break;
		}
		break;
	default:
		//nothing
		break;
	}
}

void TC8566AF::executionPhaseWrite(byte value)
{
	switch (command) {
	case CMD_WRITE_DATA:
		if (sectorOffset < sectorSize) {
			sectorBuf[sectorOffset++] = value;
			if (sectorOffset == sectorSize) {
				try {
					byte onDiskTrack;
					byte onDiskSector;
					byte onDiskSide;
					int  onDiskSize;
					drive[driveSelect]->write(
						sectorNumber, sectorBuf,
						onDiskTrack, onDiskSector,
						onDiskSide,  onDiskSize);
				} catch (MSXException& e) {
					status1 |= ST1_NW;
				}
				phase       = PHASE_RESULT;
				phaseStep   = 0;
				mainStatus |= STM_DIO;
			}
		}
		break;

	case CMD_FORMAT:
		switch (phaseStep & 3) {
		case 0:
			currentTrack = value;
			break;
		case 1:
			headNumber = value;
			memset(sectorBuf, fillerByte, sectorSize);
			try {
				byte onDiskTrack;
				byte onDiskSector;
				byte onDiskSide;
				int  onDiskSize;
				drive[driveSelect]->write(
					sectorNumber, sectorBuf,
					onDiskTrack, onDiskSector,
					onDiskSide,  onDiskSize);
			} catch (MSXException& e) {
				status1 |= ST1_NW;
			}
			break;
		case 2:
			sectorNumber = value;
			break;
		}

		if (++phaseStep == 4 * sectorsPerCylinder - 2) {
			phase       = PHASE_RESULT;
			phaseStep   = 0;
			mainStatus |= STM_DIO;
		}
		break;
	default:
		//nothing
		break;
	}
}

bool TC8566AF::diskChanged(unsigned driveNum)
{
	assert(driveNum < 4);
	return drive[driveNum]->diskChanged();
}

bool TC8566AF::peekDiskChanged(unsigned driveNum) const
{
	assert(driveNum < 4);
	return drive[driveNum]->peekDiskChanged();
}


static enum_string<TC8566AF::Command> commandInfo[] = {
	{ "UNKNOWN",                TC8566AF::CMD_UNKNOWN                },
	{ "READ_DATA",              TC8566AF::CMD_READ_DATA              },
	{ "WRITE_DATA",             TC8566AF::CMD_WRITE_DATA             },
	{ "WRITE_DELETED_DATA",     TC8566AF::CMD_WRITE_DELETED_DATA     },
	{ "READ_DELETED_DATA",      TC8566AF::CMD_READ_DELETED_DATA      },
	{ "READ_DIAGNOSTIC",        TC8566AF::CMD_READ_DIAGNOSTIC        },
	{ "READ_ID",                TC8566AF::CMD_READ_ID                },
	{ "FORMAT",                 TC8566AF::CMD_FORMAT                 },
	{ "SCAN_EQUAL",             TC8566AF::CMD_SCAN_EQUAL             },
	{ "SCAN_LOW_OR_EQUAL",      TC8566AF::CMD_SCAN_LOW_OR_EQUAL      },
	{ "SCAN_HIGH_OR_EQUAL",     TC8566AF::CMD_SCAN_HIGH_OR_EQUAL     },
	{ "SEEK",                   TC8566AF::CMD_SEEK                   },
	{ "RECALIBRATE",            TC8566AF::CMD_RECALIBRATE            },
	{ "SENSE_INTERRUPT_STATUS", TC8566AF::CMD_SENSE_INTERRUPT_STATUS },
	{ "SPECIFY",                TC8566AF::CMD_SPECIFY                },
	{ "SENSE_DEVICE_STATUS",    TC8566AF::CMD_SENSE_DEVICE_STATUS    }
};
SERIALIZE_ENUM(TC8566AF::Command, commandInfo);

static enum_string<TC8566AF::Phase> phaseInfo[] = {
	{ "IDLE",         TC8566AF::PHASE_IDLE         },
	{ "COMMAND",      TC8566AF::PHASE_COMMAND      },
	{ "DATATRANSFER", TC8566AF::PHASE_DATATRANSFER },
	{ "RESULT",       TC8566AF::PHASE_RESULT       }
};
SERIALIZE_ENUM(TC8566AF::Phase, phaseInfo);

template<typename Archive>
void TC8566AF::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("delayTime", delayTime);
	ar.serialize("command", command);
	ar.serialize("phase", phase);
	ar.serialize("phaseStep", phaseStep);
	ar.serialize("sectorSize", sectorSize);
	ar.serialize("sectorOffset", sectorOffset);
	ar.serialize_blob("sectorBuf", sectorBuf, sizeof(sectorBuf));
	ar.serialize("driveSelect", driveSelect);
	ar.serialize("mainStatus", mainStatus);
	ar.serialize("status0", status0);
	ar.serialize("status1", status1);
	ar.serialize("status2", status2);
	ar.serialize("status3", status3);
	ar.serialize("commandCode", commandCode);
	ar.serialize("cylinderNumber", cylinderNumber);
	ar.serialize("headNumber", headNumber);
	ar.serialize("sectorNumber", sectorNumber);
	ar.serialize("number", number);
	ar.serialize("currentTrack", currentTrack);
	ar.serialize("sectorsPerCylinder", sectorsPerCylinder);
	ar.serialize("fillerByte", fillerByte);
};
INSTANTIATE_SERIALIZE_METHODS(TC8566AF);

} // namespace openmsx
