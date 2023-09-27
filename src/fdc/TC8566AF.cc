/*
 * Based on code from NLMSX written by Frits Hilderink
 *        and       blueMSX written by Daniel Vik
 */

#include "TC8566AF.hh"
#include "DiskDrive.hh"
#include "RawTrack.hh"
#include "Clock.hh"
#include "MSXCliComm.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "serialize.hh"
#include "view.hh"
#include "xrange.hh"

namespace openmsx {

static constexpr uint8_t STM_DB0 = 0x01; // FDD 0 Busy
static constexpr uint8_t STM_DB1 = 0x02; // FDD 1 Busy
static constexpr uint8_t STM_DB2 = 0x04; // FDD 2 Busy
static constexpr uint8_t STM_DB3 = 0x08; // FDD 3 Busy
static constexpr uint8_t STM_CB  = 0x10; // FDC Busy
static constexpr uint8_t STM_NDM = 0x20; // Non-DMA mode
static constexpr uint8_t STM_DIO = 0x40; // Data Input/Output
static constexpr uint8_t STM_RQM = 0x80; // Request for Master

static constexpr uint8_t ST0_DS0 = 0x01; // Drive Select 0,1
static constexpr uint8_t ST0_DS1 = 0x02; //
static constexpr uint8_t ST0_HD  = 0x04; // Head Address
static constexpr uint8_t ST0_NR  = 0x08; // Not Ready
static constexpr uint8_t ST0_EC  = 0x10; // Equipment Check
static constexpr uint8_t ST0_SE  = 0x20; // Seek End
static constexpr uint8_t ST0_IC0 = 0x40; // Interrupt Code
static constexpr uint8_t ST0_IC1 = 0x80; //

static constexpr uint8_t ST1_MA  = 0x01; // Missing Address Mark
static constexpr uint8_t ST1_NW  = 0x02; // Not Writable
static constexpr uint8_t ST1_ND  = 0x04; // No Data
//                               = 0x08; // -
static constexpr uint8_t ST1_OR  = 0x10; // Over Run
static constexpr uint8_t ST1_DE  = 0x20; // Data Error
//                               = 0x40; // -
static constexpr uint8_t ST1_EN  = 0x80; // End of Cylinder

static constexpr uint8_t ST2_MD  = 0x01; // Missing Address Mark in Data Field
static constexpr uint8_t ST2_BC  = 0x02; // Bad Cylinder
static constexpr uint8_t ST2_SN  = 0x04; // Scan Not Satisfied
static constexpr uint8_t ST2_SH  = 0x08; // Scan Equal Satisfied
static constexpr uint8_t ST2_NC  = 0x10; // No cylinder
static constexpr uint8_t ST2_DD  = 0x20; // Data Error in Data Field
static constexpr uint8_t ST2_CM  = 0x40; // Control Mark
//                               = 0x80; // -

static constexpr uint8_t ST3_DS0 = 0x01; // Drive Select 0
static constexpr uint8_t ST3_DS1 = 0x02; // Drive Select 1
static constexpr uint8_t ST3_HD  = 0x04; // Head Address
static constexpr uint8_t ST3_2S  = 0x08; // Two Side
static constexpr uint8_t ST3_TK0 = 0x10; // Track 0
static constexpr uint8_t ST3_RDY = 0x20; // Ready
static constexpr uint8_t ST3_WP  = 0x40; // Write Protect
static constexpr uint8_t ST3_FLT = 0x80; // Fault


TC8566AF::TC8566AF(Scheduler& scheduler_, std::span<std::unique_ptr<DiskDrive>, 4> drv, MSXCliComm& cliComm_,
                   EmuTime::param time)
	: Schedulable(scheduler_)
	, cliComm(cliComm_)
{
	setDrqRate(RawTrack::STANDARD_SIZE);

	ranges::copy(view::transform(drv, [](auto& p) { return p.get(); }),
	             drive);
	reset(time);
}

void TC8566AF::reset(EmuTime::param time)
{
	for (auto* d : drive) {
		d->setMotor(false, time);
	}
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
	endOfTrack = 0;
	sectorsPerCylinder = 0;
	fillerByte = 0;
	gapLength = 0;
	specifyData[0] = 0; // TODO check
	specifyData[1] = 0; // TODO check
	for (auto& si : seekInfo) {
		si.time = EmuTime::zero();
		si.currentTrack = 0;
		si.seekValue = 0;
		si.state = SEEK_IDLE;
	}
	headUnloadTime = EmuTime::zero(); // head not loaded

	mainStatus = STM_RQM;
	//interrupt = false;
}

uint8_t TC8566AF::peekStatus() const
{
	bool nonDMAMode = specifyData[1] & 1;
	bool dma = nonDMAMode && (phase == PHASE_DATA_TRANSFER);
	return mainStatus | (dma ? STM_NDM : 0);
}

uint8_t TC8566AF::readStatus(EmuTime::param time)
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

uint8_t TC8566AF::peekDataPort(EmuTime::param time) const
{
	switch (phase) {
	case PHASE_DATA_TRANSFER:
		return executionPhasePeek(time);
	case PHASE_RESULT:
		return resultsPhasePeek();
	default:
		return 0xff;
	}
}

uint8_t TC8566AF::readDataPort(EmuTime::param time)
{
	//interrupt = false;
	switch (phase) {
	case PHASE_DATA_TRANSFER:
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

uint8_t TC8566AF::executionPhasePeek(EmuTime::param time) const
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

uint8_t TC8566AF::executionPhaseRead(EmuTime::param time)
{
	switch (command) {
	case CMD_READ_DATA: {
		assert(dataAvailable);
		auto* drv = drive[driveSelect];
		uint8_t result = drv->readTrackByte(dataCurrent++);
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
			uint16_t diskCrc  = 256 * drv->readTrackByte(dataCurrent++);
			         diskCrc +=       drv->readTrackByte(dataCurrent++);
			if (diskCrc != crc.getValue()) {
				status0 |= ST0_IC0;
				status1 |= ST1_DE;
				status2 |= ST2_DD;
				resultPhase();
			} else {
				++sectorNumber;
				if (sectorNumber > endOfTrack) {
					// done
					resultPhase();
				} else {
					// read next sector
					startReadWriteSector(time);
				}
			}
		}
		return result;
	}
	default:
		return 0xff;
	}
}

uint8_t TC8566AF::resultsPhasePeek() const
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
			return seekInfo[status0 & 3].currentTrack;
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

uint8_t TC8566AF::resultsPhaseRead(EmuTime::param time)
{
	uint8_t result = resultsPhasePeek();
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
		case 0:
			status0 = 0; // TODO correct?  Reset _all_ bits?
			break;
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

void TC8566AF::writeControlReg0(uint8_t value, EmuTime::param time)
{
	drive[3]->setMotor((value & 0x80) != 0, time);
	drive[2]->setMotor((value & 0x40) != 0, time);
	drive[1]->setMotor((value & 0x20) != 0, time);
	drive[0]->setMotor((value & 0x10) != 0, time);
	//enableIntDma = value & 0x08;
	//notReset     = value & 0x04;
	driveSelect = value & 0x03;
}

void TC8566AF::writeControlReg1(uint8_t value, EmuTime::param /*time*/)
{
	if (value & 1) { // TC, terminate multi-sector read/write command
		if (phase == PHASE_DATA_TRANSFER) {
			resultPhase();
		}
	}
}

void TC8566AF::writeDataPort(uint8_t value, EmuTime::param time)
{
	switch (phase) {
	case PHASE_IDLE:
		idlePhaseWrite(value, time);
		break;

	case PHASE_COMMAND:
		commandPhaseWrite(value, time);
		break;

	case PHASE_DATA_TRANSFER:
		executionPhaseWrite(value, time);
		break;
	default:
		// nothing
		break;
	}
}

void TC8566AF::idlePhaseWrite(uint8_t value, EmuTime::param time)
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

void TC8566AF::commandPhase1(uint8_t value)
{
	drive[driveSelect]->setSide((value & 0x04) != 0);
	status0 &= ~(ST0_DS0 | ST0_DS1 | ST0_IC0 | ST0_IC1);
	status0 |= uint8_t(
		   //(drive[driveSelect]->isDiskInserted() ? 0 : ST0_DS0) |
	           (value & (ST0_DS0 | ST0_DS1)) |
	           (drive[driveSelect]->isDummyDrive() ? ST0_IC1 : 0));
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

void TC8566AF::commandPhaseWrite(uint8_t value, EmuTime::param time)
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
			endOfTrack = value;
			break;
		case 6: // Gap Length
			// ignore
			break;
		case 7: // Data length
			// ignore value
			startReadWriteSector(time);
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
			phase        = PHASE_DATA_TRANSFER;
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
		case 1: {
			endCommand(time);
			auto n = status0 & 3;
			auto& si = seekInfo[n];
			si.time = time;
			si.seekValue = value; // target track
			si.state = SEEK_SEEK;
			doSeek(n);
			break;
		}
		}
		break;

	case CMD_RECALIBRATE:
		switch (phaseStep++) {
		case 0: {
			commandPhase1(value);
			endCommand(time);
			int n = status0 & 3;
			auto& si = seekInfo[n];
			si.time = time;
			si.seekValue = 255; // max try 255 steps
			si.state = SEEK_RECALIBRATE;
			doSeek(n);
			break;
		}
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

void TC8566AF::startReadWriteSector(EmuTime::param time)
{
	phase = PHASE_DATA_TRANSFER;
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
}

void TC8566AF::initTrackHeader(EmuTime::param time)
{
	try {
		auto* drv = drive[driveSelect];
		auto trackLength = drv->getTrackLength();
		setDrqRate(trackLength);
		dataCurrent = 0;
		dataAvailable = trackLength;

		auto write = [&](unsigned n, uint8_t value) {
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

	auto write1 = [&](uint8_t value, bool idam = false) {
		drv->writeTrackByte(dataCurrent++, value, idam);
	};
	auto writeU = [&](uint8_t value) {
		write1(value);
		crc.update(value);
	};
	auto writeN = [&](unsigned n, uint8_t value) {
		repeat(n, [&] { write1(value); });
	};
	auto writeCRC = [&] {
		write1(narrow_cast<uint8_t>(crc.getValue() >> 8));   // CRC (high byte)
		write1(narrow_cast<uint8_t>(crc.getValue() & 0xff)); //     (low  byte)
	};

	writeN(12, 0x00); // sync

	writeN(3, 0xA1); // addr mark
	write1(0xFE, true); // addr mark + add idam
	crc.init({0xA1, 0xA1, 0xA1, 0xFE});
	writeU(cylinderNumber); // C: Cylinder number
	writeU(headNumber);     // H: Head Address
	writeU(sectorNumber);   // R: Record
	writeU(number);         // N: Length of sector
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

void TC8566AF::doSeek(int n)
{
	auto& si = seekInfo[n];
	DiskDrive& currentDrive = *drive[n];

	const auto stm_dbn = uint8_t(1 << n); // STM_DB0..STM_DB3
	mainStatus |= stm_dbn;

	auto endSeek = [&] {
		status0 |= ST0_SE;
		si.state = SEEK_IDLE;
		mainStatus &= ~stm_dbn;
	};

	if (currentDrive.isDummyDrive()) {
		status0 |= ST0_NR;
		endSeek();
		return;
	}

	bool direction = false; // initialize to avoid warning
	switch (si.state) {
	case SEEK_SEEK:
		if (si.seekValue > si.currentTrack) {
			++si.currentTrack;
			direction = true;
		} else if (si.seekValue < si.currentTrack) {
			--si.currentTrack;
			direction = false;
		} else {
			assert(si.seekValue == si.currentTrack);
			endSeek();
			return;
		}
		break;
	case SEEK_RECALIBRATE:
		if (currentDrive.isTrack00() || (si.seekValue == 0)) {
			if (si.seekValue == 0) {
				status0 |= ST0_EC;
			}
			si.currentTrack = 0;
			endSeek();
			return;
		}
		direction = false;
		--si.seekValue;
		break;
	default:
		UNREACHABLE;
	}

	currentDrive.step(direction, si.time);

	si.time += getSeekDelay();
	setSyncPoint(si.time);
}

void TC8566AF::executeUntil(EmuTime::param time)
{
	for (auto n : xrange(4)) {
		if ((seekInfo[n].state != SEEK_IDLE) &&
		    (seekInfo[n].time == time)) {
			doSeek(n);
		}
	}
}

void TC8566AF::writeSector()
{
	// write 2 CRC bytes (big endian)
	auto* drv = drive[driveSelect];
	drv->writeTrackByte(dataCurrent++, narrow_cast<uint8_t>(crc.getValue() >> 8));
	drv->writeTrackByte(dataCurrent++, narrow_cast<uint8_t>(crc.getValue() & 0xFF));
	drv->flushTrack();
}

void TC8566AF::executionPhaseWrite(uint8_t value, EmuTime::param time)
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

				++sectorNumber;
				if (sectorNumber > endOfTrack) {
					// done
					resultPhase();
				} else {
					// write next sector
					startReadWriteSector(time);
				}
			} catch (MSXException&) {
				status0 |= ST0_IC0;
				status1 |= ST1_NW;
				resultPhase();
			}
		}
		break;

	case CMD_FORMAT:
		delayTime += 1; // correct?
		mainStatus &= ~STM_RQM;
		switch (phaseStep & 3) {
		case 0:
			cylinderNumber = value;
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
	{ "DATATRANSFER", TC8566AF::PHASE_DATA_TRANSFER },
	{ "RESULT",       TC8566AF::PHASE_RESULT       }
};
SERIALIZE_ENUM(TC8566AF::Phase, phaseInfo);

static constexpr std::initializer_list<enum_string<TC8566AF::SeekState>> seekInfo = {
	{ "IDLE",        TC8566AF::SEEK_IDLE },
	{ "SEEK",        TC8566AF::SEEK_SEEK },
	{ "RECALIBRATE", TC8566AF::SEEK_RECALIBRATE }
};
SERIALIZE_ENUM(TC8566AF::SeekState, seekInfo);

template<typename Archive>
void TC8566AF::SeekInfo::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("time",         time,
	             "currentTrack", currentTrack,
	             "seekValue",    seekValue,
	             "state",        state);
}

// version 1: initial version
// version 2: added specifyData, headUnloadTime, seekValue
//            inherit from Schedulable
// version 3: Replaced 'sectorSize', 'sectorOffset', 'sectorBuf'
//            with 'dataAvailable', 'dataCurrent', .trackData'.
//            Not 100% backwards compatible, see also comments in WD2793.
//            Added 'crc' and 'gapLength'.
// version 4: changed type of delayTime from Clock to DynamicClock
// version 5: removed trackData
// version 6: added seekInfo[4]
// version 7: added 'endOfTrack'
template<typename Archive>
void TC8566AF::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("delayTime", delayTime);
	} else {
		assert(Archive::IS_LOADER);
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
	             "sectorsPerCylinder", sectorsPerCylinder,
	             "fillerByte",         fillerByte);
	if (ar.versionAtLeast(version, 2)) {
		ar.template serializeBase<Schedulable>(*this);
		ar.serialize("specifyData",    specifyData,
		             "headUnloadTime", headUnloadTime);
	} else {
		assert(Archive::IS_LOADER);
		specifyData[0] = 0xDF; // values normally set by TurboR disk rom
		specifyData[1] = 0x03;
		headUnloadTime = EmuTime::zero();
	}
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("dataAvailable", dataAvailable,
		             "dataCurrent", dataCurrent,
		             "gapLength", gapLength);
		uint16_t crcVal = crc.getValue();
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
	if (ar.versionAtLeast(version, 6)) {
		ar.serialize("seekInfo", seekInfo);
	} else {
		if (command == one_of(CMD_SEEK, CMD_RECALIBRATE)) {
			cliComm.printWarning(
				"Loading an old savestate that has an "
				"in-progress TC8566AF seek-command. This is "
				"not fully backwards-compatible and can cause "
				"wrong emulation behavior.");
		}
		uint8_t currentTrack = 0;
		ar.serialize("currentTrack", currentTrack);
		for (auto& si : seekInfo) {
			si.currentTrack = currentTrack;
			assert(si.state == SEEK_IDLE);
		}
	}
	if (ar.versionAtLeast(version, 7)) {
		ar.serialize("endOfTrack", endOfTrack);
	}
};
INSTANTIATE_SERIALIZE_METHODS(TC8566AF);

} // namespace openmsx
