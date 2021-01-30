/*
 * Based on code from NLMSX written by Frits Hilderink
 *        and       blueMSX written by Daniel Vik
 */

#include "TC8566AF.hh"
#include "DiskDrive.hh"
#include "RawTrack.hh"
#include "Clock.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

constexpr byte STM_DB0 = 0x01; // FDD 0 Busy
constexpr byte STM_DB1 = 0x02; // FDD 1 Busy
constexpr byte STM_DB2 = 0x04; // FDD 2 Busy
constexpr byte STM_DB3 = 0x08; // FDD 3 Busy
constexpr byte STM_CB  = 0x10; // FDC Busy
constexpr byte STM_NDM = 0x20; // Non-DMA mode
constexpr byte STM_DIO = 0x40; // Data Input/Output
constexpr byte STM_RQM = 0x80; // Request for Master

constexpr byte ST0_DS0 = 0x01; // Drive Select 0,1
constexpr byte ST0_DS1 = 0x02; //
constexpr byte ST0_HD  = 0x04; // Head Address
constexpr byte ST0_NR  = 0x08; // Not Ready
constexpr byte ST0_EC  = 0x10; // Equipment Check
constexpr byte ST0_SE  = 0x20; // Seek End
constexpr byte ST0_IC0 = 0x40; // Interrupt Code
constexpr byte ST0_IC1 = 0x80; //

constexpr byte ST1_MA  = 0x01; // Missing Address Mark
constexpr byte ST1_NW  = 0x02; // Not Writable
constexpr byte ST1_ND  = 0x04; // No Data
//                        = 0x08; // -
constexpr byte ST1_OR  = 0x10; // Over Run
constexpr byte ST1_DE  = 0x20; // Data Error
//                        = 0x40; // -
constexpr byte ST1_EN  = 0x80; // End of Cylinder

constexpr byte ST2_MD  = 0x01; // Missing Address Mark in Data Field
constexpr byte ST2_BC  = 0x02; // Bad Cylinder
constexpr byte ST2_SN  = 0x04; // Scan Not Satisfied
constexpr byte ST2_SH  = 0x08; // Scan Equal Satisfied
constexpr byte ST2_NC  = 0x10; // No cylinder
constexpr byte ST2_DD  = 0x20; // Data Error in Data Field
constexpr byte ST2_CM  = 0x40; // Control Mark
//                        = 0x80; // -

constexpr byte ST3_DS0 = 0x01; // Drive Select 0
constexpr byte ST3_DS1 = 0x02; // Drive Select 1
constexpr byte ST3_HD  = 0x04; // Head Address
constexpr byte ST3_2S  = 0x08; // Two Side
constexpr byte ST3_TK0 = 0x10; // Track 0
constexpr byte ST3_RDY = 0x20; // Ready
constexpr byte ST3_WP  = 0x40; // Write Protect
constexpr byte ST3_FLT = 0x80; // Fault


TC8566AF::TC8566AF(Scheduler& scheduler_, DiskDrive* drv[4], CliComm& cliComm_,
                   EmuTime::param time)
	: Schedulable(scheduler_)
	, cliComm(cliComm_)
	, delayTime(EmuTime::zero())
	, headUnloadTime(EmuTime::zero()) // head not loaded
{
	// avoid UMR (on savestate)
	dataAvailable = 0;
	dataCurrent = 0;
	setDrqRate(RawTrack::STANDARD_SIZE);

	drive[0] = drv[0];
	drive[1] = drv[1];
	drive[2] = drv[2];
	drive[3] = drv[3];
	reset(time);
}

void TC8566AF::reset(EmuTime::param time)
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
	gapLength = 0;
	specifyData[0] = 0; // TODO check
	specifyData[1] = 0; // TODO check
	seekValue = 0;
	headUnloadTime = EmuTime::zero(); // head not loaded

	mainStatus = STM_RQM;
	//interrupt = false;
}

byte TC8566AF::peekReg(int reg, EmuTime::param time) const
{
	switch (reg) {
	case 4: // Main Status Register
		return peekStatus();
	case 5: // data port
		return peekDataPort(time);
	}
	return 0xff;
}

byte TC8566AF::readReg(int reg, EmuTime::param time)
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
	bool nonDMAMode = specifyData[1] & 1;
	bool dma = nonDMAMode && (phase == PHASE_DATATRANSFER);
	return mainStatus | (dma ? STM_NDM : 0);
}

byte TC8566AF::readStatus(EmuTime::param time)
{
	if (delayTime.before(time)) {
		mainStatus |= STM_RQM;
	}
	return peekStatus();
}

void TC8566AF::setDrqRate(unsigned trackLength)
{
	delayTime.setFreq(trackLength * DiskDrive::ROTATIONS_PER_SECOND);
}

byte TC8566AF::peekDataPort(EmuTime::param time) const
{
	switch (phase) {
	case PHASE_DATATRANSFER:
		return executionPhasePeek(time);
	case PHASE_RESULT:
		return resultsPhasePeek();
	default:
		return 0xff;
	}
}

byte TC8566AF::readDataPort(EmuTime::param time)
{
	//interrupt = false;
	switch (phase) {
	case PHASE_DATATRANSFER:
		if (delayTime.before(time)) {
			return executionPhaseRead(time);
		} else {
			return 0xff; // TODO check this
		}
	case PHASE_RESULT:
		return resultsPhaseRead(time);
	default:
		return 0xff;
	}
}

byte TC8566AF::executionPhasePeek(EmuTime::param time) const
{
	switch (command) {
	case CMD_READ_DATA:
		if (delayTime.before(time)) {
			assert(dataAvailable);
			return drive[driveSelect]->readTrackByte(dataCurrent);
		} else {
			return 0xff; // TODO check this
		}
	default:
		return 0xff;
	}
}

byte TC8566AF::executionPhaseRead(EmuTime::param time)
{
	switch (command) {
	case CMD_READ_DATA: {
		assert(dataAvailable);
		auto* drv = drive[driveSelect];
		byte result = drv->readTrackByte(dataCurrent++);
		crc.update(result);
		--dataAvailable;
		delayTime += 1; // time when next byte will be available
		mainStatus &= ~STM_RQM;
		if (delayTime.before(time)) {
			// lost data
			status0 |= ST0_IC0;
			status1 |= ST1_OR;
			resultPhase();
		} else if (!dataAvailable) {
			// check crc error
			word diskCrc  = 256 * drv->readTrackByte(dataCurrent++);
			     diskCrc +=       drv->readTrackByte(dataCurrent++);
			if (diskCrc != crc.getValue()) {
				status0 |= ST0_IC0;
				status1 |= ST1_DE;
				status2 |= ST2_DD;
			}
			resultPhase();
		}
		return result;
	}
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

byte TC8566AF::resultsPhaseRead(EmuTime::param time)
{
	byte result = resultsPhasePeek();
	switch (command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		switch (phaseStep++) {
		case 6:
			endCommand(time);
			break;
		}
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		switch (phaseStep++) {
		case 1:
			endCommand(time);
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (phaseStep++) {
		case 0:
			endCommand(time);
			break;
		}
		break;
	default:
		// nothing
		break;
	}
	return result;
}

void TC8566AF::writeReg(int reg, byte data, EmuTime::param time)
{
	switch (reg) {
	case 2: // control register 0
		drive[3]->setMotor((data & 0x80) != 0, time);
		drive[2]->setMotor((data & 0x40) != 0, time);
		drive[1]->setMotor((data & 0x20) != 0, time);
		drive[0]->setMotor((data & 0x10) != 0, time);
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

void TC8566AF::writeDataPort(byte value, EmuTime::param time)
{
	switch (phase) {
	case PHASE_IDLE:
		idlePhaseWrite(value, time);
		break;

	case PHASE_COMMAND:
		commandPhaseWrite(value, time);
		break;

	case PHASE_DATATRANSFER:
		executionPhaseWrite(value, time);
		break;
	default:
		// nothing
		break;
	}
}

void TC8566AF::idlePhaseWrite(byte value, EmuTime::param time)
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
		status1 = 0;
		status2 = 0;
		//MT  = value & 0x80;
		//MFM = value & 0x40;
		//SK  = value & 0x20;
		break;

	case CMD_RECALIBRATE:
		status0 &= ~ST0_SE;
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		resultPhase();
		break;

	case CMD_SEEK:
	case CMD_SPECIFY:
	case CMD_SENSE_DEVICE_STATUS:
		break;

	default:
		endCommand(time);
	}
}

void TC8566AF::commandPhase1(byte value)
{
	drive[driveSelect]->setSide((value & 0x04) != 0);
	status0 &= ~(ST0_DS0 | ST0_DS1 | ST0_IC0 | ST0_IC1);
	status0 |= //(drive[driveSelect]->isDiskInserted() ? 0 : ST0_DS0) |
	           (value & (ST0_DS0 | ST0_DS1)) |
	           (drive[driveSelect]->isDummyDrive() ? ST0_IC1 : 0);
	status3  = (value & (ST3_DS0 | ST3_DS1)) |
	           (drive[driveSelect]->isTrack00()        ? ST3_TK0 : 0) |
	           (drive[driveSelect]->isDoubleSided()    ? ST3_HD  : 0) |
	           (drive[driveSelect]->isWriteProtected() ? ST3_WP  : 0) |
	           (drive[driveSelect]->isDiskInserted()   ? ST3_RDY : 0);
}

EmuTime TC8566AF::locateSector(EmuTime::param time)
{
	RawTrack::Sector sectorInfo;
	int lastIdx = -1;
	EmuTime next = time;
	while (true) {
		try {
			auto* drv = drive[driveSelect];
			setDrqRate(drv->getTrackLength());
			next = drv->getNextSector(next, sectorInfo);
		} catch (MSXException& /*e*/) {
			return EmuTime::infinity();
		}
		if ((next == EmuTime::infinity()) ||
		    (sectorInfo.addrIdx == lastIdx)) {
			// no sectors on track or sector already seen
			return EmuTime::infinity();
		}
		if (lastIdx == -1) lastIdx = sectorInfo.addrIdx;
		if (sectorInfo.addrCrcErr)               continue;
		if (sectorInfo.track  != cylinderNumber) continue;
		if (sectorInfo.head   != headNumber)     continue;
		if (sectorInfo.sector != sectorNumber)   continue;
		if (sectorInfo.dataIdx == -1)            continue;
		break;
	}
	// TODO does TC8566AF look at lower 3 bits?
	dataAvailable = 128 << (sectorInfo.sizeCode & 7);
	dataCurrent = sectorInfo.dataIdx;
	return next;
}

void TC8566AF::commandPhaseWrite(byte value, EmuTime::param time)
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
			break;
		case 5: // End Of Track
			break;
		case 6: // Gap Length
			break;
		case 7: // Data length
			phase = PHASE_DATATRANSFER;
			phaseStep = 0;
			//interrupt = true;

			// load drive head, if not already loaded
			EmuTime ready = time;
			if (!isHeadLoaded(time)) {
				ready += getHeadLoadDelay();
				// set 'head is loaded'
				headUnloadTime = EmuTime::infinity();
			}

			// actually read sector: fills in
			//   dataAvailable and dataCurrent
			ready = locateSector(ready);
			if (ready == EmuTime::infinity()) {
				status0 |= ST0_IC0;
				status1 |= ST1_ND;
				resultPhase();
				return;
			}
			if (command == CMD_READ_DATA) {
				mainStatus |= STM_DIO;
			} else {
				mainStatus &= ~STM_DIO;
			}
			// Initialize crc
			// TODO 0xFB vs 0xF8 depends on deleted vs normal data
			crc.init({0xA1, 0xA1, 0xA1, 0xFB});

			// first byte is available when it's rotated below the
			// drive-head
			delayTime.reset(ready);
			mainStatus &= ~STM_RQM;
			break;
		}
		break;

	case CMD_FORMAT:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			break;
		case 1:
			number = value;
			break;
		case 2:
			sectorsPerCylinder = value;
			sectorNumber       = value;
			break;
		case 3:
			gapLength = value;
			break;
		case 4:
			fillerByte   = value;
			mainStatus  &= ~STM_DIO;
			phase        = PHASE_DATATRANSFER;
			phaseStep    = 0;
			//interrupt    = true;
			initTrackHeader(time);
			break;
		}
		break;

	case CMD_SEEK:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			break;
		case 1:
			seekValue = value; // target track
			doSeek(time);
			break;
		}
		break;

	case CMD_RECALIBRATE:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			seekValue = 255; // max try 255 steps
			doSeek(time);
			break;
		}
		break;

	case CMD_SPECIFY:
		specifyData[phaseStep] = value;
		switch (phaseStep++) {
		case 1:
			endCommand(time);
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (phaseStep++) {
		case 0:
			commandPhase1(value);
			resultPhase();
			break;
		}
		break;
	default:
		// nothing
		break;
	}
}

void TC8566AF::initTrackHeader(EmuTime::param time)
{
	try {
		auto* drv = drive[driveSelect];
		auto trackLength = drv->getTrackLength();
		setDrqRate(trackLength);
		dataCurrent = 0;
		dataAvailable = trackLength;

		auto write = [&](unsigned n, byte value) {
			repeat(n, [&] { drv->writeTrackByte(dataCurrent++, value); });
		};
		write(80, 0x4E); // gap4a
		write(12, 0x00); // sync
		write( 3, 0xC2); // index mark
		write( 1, 0xFC); //   "    "
		write(50, 0x4E); // gap1
	} catch (MSXException& /*e*/) {
		endCommand(time);
	}
}

void TC8566AF::formatSector()
{
	auto* drv = drive[driveSelect];

	auto write1 = [&](byte value, bool idam = false) {
		drv->writeTrackByte(dataCurrent++, value, idam);
	};
	auto writeU = [&](byte value) {
		write1(value);
		crc.update(value);
	};
	auto writeN = [&](unsigned n, byte value) {
		repeat(n, [&] { write1(value); });
	};
	auto writeCRC = [&] {
		write1(crc.getValue() >> 8);   // CRC (high byte)
		write1(crc.getValue() & 0xff); //     (low  byte)
	};

	writeN(12, 0x00); // sync

	writeN(3, 0xA1); // addr mark
	write1(0xFE, true); // addr mark + add idam
	crc.init({0xA1, 0xA1, 0xA1, 0xFE});
	writeU(currentTrack); // C: Cylinder number
	writeU(headNumber);   // H: Head Address
	writeU(sectorNumber); // R: Record
	writeU(number);       // N: Length of sector
	writeCRC();

	writeN(22, 0x4E); // gap2
	writeN(12, 0x00); // sync

	writeN(3, 0xA1); // data mark
	write1(0xFB);   //  "    "
	crc.init({0xA1, 0xA1, 0xA1, 0xFB});
	repeat(128 << (number & 7), [&] { writeU(fillerByte); });
	writeCRC();

	writeN(gapLength, 0x4E); // gap3
}

void TC8566AF::doSeek(EmuTime::param time)
{
	DiskDrive& currentDrive = *drive[driveSelect];

	bool direction = false; // initialize to avoid warning
	switch (command) {
	case CMD_SEEK:
		if (seekValue > currentTrack) {
			++currentTrack;
			direction = true;
		} else if (seekValue < currentTrack) {
			--currentTrack;
			direction = false;
		} else {
			assert(seekValue == currentTrack);
			status0 |= ST0_SE;
			endCommand(time);
			return;
		}
		break;
	case CMD_RECALIBRATE:
		if (currentDrive.isTrack00() || (seekValue == 0)) {
			if (seekValue == 0) {
				status0 |= ST0_EC;
			}
			currentTrack = 0;
			status0 |= ST0_SE;
			endCommand(time);
			return;
		}
		direction = false;
		--seekValue;
		break;
	default:
		UNREACHABLE;
	}

	currentDrive.step(direction, time);

	setSyncPoint(time + getSeekDelay());
}

void TC8566AF::executeUntil(EmuTime::param time)
{
	if (command == one_of(CMD_SEEK, CMD_RECALIBRATE)) {
		doSeek(time);
	}
}

void TC8566AF::writeSector()
{
	// write 2 CRC bytes (big endian)
	auto* drv = drive[driveSelect];
	drv->writeTrackByte(dataCurrent++, crc.getValue() >> 8);
	drv->writeTrackByte(dataCurrent++, crc.getValue() & 0xFF);
	drv->flushTrack();
}

void TC8566AF::executionPhaseWrite(byte value, EmuTime::param time)
{
	auto* drv = drive[driveSelect];
	switch (command) {
	case CMD_WRITE_DATA:
		assert(dataAvailable);
		drv->writeTrackByte(dataCurrent++, value);
		crc.update(value);
		--dataAvailable;
		delayTime += 1; // time when next byte can be written
		mainStatus &= ~STM_RQM;
		if (delayTime.before(time)) {
			// lost data
			status0 |= ST0_IC0;
			status1 |= ST1_OR;
			resultPhase();
		} else if (!dataAvailable) {
			try {
				writeSector();
			} catch (MSXException&) {
				status0 |= ST0_IC0;
				status1 |= ST1_NW;
			}
			resultPhase();
		}
		break;

	case CMD_FORMAT:
		delayTime += 1; // correct?
		mainStatus &= ~STM_RQM;
		switch (phaseStep & 3) {
		case 0:
			currentTrack = value;
			break;
		case 1:
			headNumber = value;
			break;
		case 2:
			sectorNumber = value;
			break;
		case 3:
			number = value;
			formatSector();
			break;
		}
		++phaseStep;

		if (phaseStep == 4 * sectorsPerCylinder) {
			// data for all sectors was written, now write track
			try {
				drv->flushTrack();
			} catch (MSXException&) {
				status0 |= ST0_IC0;
				status1 |= ST1_NW;
			}
			resultPhase();
		}
		break;
	default:
		// nothing
		break;
	}
}

void TC8566AF::resultPhase()
{
	mainStatus |= STM_DIO | STM_RQM;
	phase       = PHASE_RESULT;
	phaseStep   = 0;
	//interrupt = true;
}

void TC8566AF::endCommand(EmuTime::param time)
{
	phase       = PHASE_IDLE;
	mainStatus &= ~(STM_CB | STM_DIO);
	delayTime.reset(time); // set STM_RQM
	if (headUnloadTime == EmuTime::infinity()) {
		headUnloadTime = time + getHeadUnloadDelay();
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


bool TC8566AF::isHeadLoaded(EmuTime::param time) const
{
	return time < headUnloadTime;
}
EmuDuration TC8566AF::getHeadLoadDelay() const
{
	return EmuDuration::msec(2 * (specifyData[1] >> 1)); // 2ms per unit
}
EmuDuration TC8566AF::getHeadUnloadDelay() const
{
	return EmuDuration::msec(16 * (specifyData[0] & 0x0F)); // 16ms per unit
}

EmuDuration TC8566AF::getSeekDelay() const
{
	return EmuDuration::msec(16 - (specifyData[0] >> 4)); // 1ms per unit
}


static constexpr std::initializer_list<enum_string<TC8566AF::Command>> commandInfo = {
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

static constexpr std::initializer_list<enum_string<TC8566AF::Phase>> phaseInfo = {
	{ "IDLE",         TC8566AF::PHASE_IDLE         },
	{ "COMMAND",      TC8566AF::PHASE_COMMAND      },
	{ "DATATRANSFER", TC8566AF::PHASE_DATATRANSFER },
	{ "RESULT",       TC8566AF::PHASE_RESULT       }
};
SERIALIZE_ENUM(TC8566AF::Phase, phaseInfo);

// version 1: initial version
// version 2: added specifyData, headUnloadTime, seekValue
//            inherit from Schedulable
// version 3: Replaced 'sectorSize', 'sectorOffset', 'sectorBuf'
//            with 'dataAvailable', 'dataCurrent', .trackData'.
//            Not 100% backwardscompatible, see also comments in WD2793.
//            Added 'crc' and 'gapLength'.
// version 4: changed type of delayTime from Clock to DynamicClock
// version 5: removed trackData
template<typename Archive>
void TC8566AF::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("delayTime", delayTime);
	} else {
		assert(ar.isLoader());
		Clock<6250 * 5> c(EmuTime::dummy());
		ar.serialize("delayTime", c);
		delayTime.reset(c.getTime());
		delayTime.setFreq(6250 * 5);
	}
	ar.serialize("command",            command,
	             "phase",              phase,
	             "phaseStep",          phaseStep,
	             "driveSelect",        driveSelect,
	             "mainStatus",         mainStatus,
	             "status0",            status0,
	             "status1",            status1,
	             "status2",            status2,
	             "status3",            status3,
	             "commandCode",        commandCode,
	             "cylinderNumber",     cylinderNumber,
	             "headNumber",         headNumber,
	             "sectorNumber",       sectorNumber,
	             "number",             number,
	             "currentTrack",       currentTrack,
	             "sectorsPerCylinder", sectorsPerCylinder,
	             "fillerByte",         fillerByte);
	if (ar.versionAtLeast(version, 2)) {
		ar.template serializeBase<Schedulable>(*this);
		ar.serialize("specifyData",    specifyData,
		             "headUnloadTime", headUnloadTime,
		             "seekValue",      seekValue);
	} else {
		assert(ar.isLoader());
		specifyData[0] = 0xDF; // values normally set by TurboR disk rom
		specifyData[1] = 0x03;
		headUnloadTime = EmuTime::zero();
		seekValue = 0;
	}
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("dataAvailable", dataAvailable,
		             "dataCurrent", dataCurrent,
		             "gapLength", gapLength);
		word crcVal = crc.getValue();
		ar.serialize("crc", crcVal);
		crc.init(crcVal);
	}
	if (ar.versionBelow(version, 5)) {
		// Version 4->5: 'trackData' moved from FDC to RealDrive.
		if (phase != PHASE_IDLE) {
			cliComm.printWarning(
				"Loading an old savestate that has an "
				"in-progress TC8566AF command. This is not "
				"fully backwards-compatible and can cause "
				"wrong emulation behavior.");
		}
	}
};
INSTANTIATE_SERIALIZE_METHODS(TC8566AF);

} // namespace openmsx
