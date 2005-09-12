// $Id$

/*
 * Based on code from NLMSX written by Frits Hilderink
 * Format command based on code from BlueMSX written by Daniel Vik
 */

#include "TC8566AF.hh"
#include "DiskDrive.hh"
#include "MSXException.hh"

namespace openmsx {

const byte PHASE_IDLE         = 0;
const byte PHASE_COMMAND      = 1;
const byte PHASE_DATATRANSFER = 2;
const byte PHASE_RESULT       = 3;

const byte CMD_UNKNOWN                =  0;
const byte CMD_READ_DATA              =  1;
const byte CMD_WRITE_DATA             =  2;
const byte CMD_WRITE_DELETED_DATA     =  3;
const byte CMD_READ_DELETED_DATA      =  4;
const byte CMD_READ_DIAGNOSTIC        =  5;
const byte CMD_READ_ID                =  6;
const byte CMD_FORMAT                 =  7;
const byte CMD_SCAN_EQUAL             =  8;
const byte CMD_SCAN_LOW_OR_EQUAL      =  9;
const byte CMD_SCAN_HIGH_OR_EQUAL     = 10;
const byte CMD_SEEK                   = 11;
const byte CMD_RECALIBRATE            = 12;
const byte CMD_SENSE_INTERRUPT_STATUS = 13;
const byte CMD_SPECIFY                = 14;
const byte CMD_SENSE_DEVICE_STATUS    = 15;


TC8566AF::TC8566AF(DiskDrive* drv[4], const EmuTime& time)
{
	drive[0] = drv[0];
	drive[1] = drv[1];
	drive[2] = drv[2];
	drive[3] = drv[3];
	reset(time);
}

TC8566AF::~TC8566AF()
{
}

void TC8566AF::reset(const EmuTime& time)
{
	// Control register 0
	drive[0]->setMotor(false, time);
	drive[1]->setMotor(false, time);
	drive[2]->setMotor(false, time);
	drive[3]->setMotor(false, time);
	EnableIntDma = 0;	// always 0
	NotReset = 1;
	DriveSelect = 0;	// drive select: 0 - 3

	// Main Status Register
	RequestForMaster = 1;
	dataInputOutput = 0;	// 1 = read from data register to CPU
				// 0 = write from CPU to data register
	NonDMAMode = 1;		// 1 = Non DMA Mode
	FDCBusy = 0;		// Execution/Command/Result phase
	FDD3Busy = 0;
	FDD2Busy = 0;
	FDD1Busy = 0;
	FDD0Busy = 0;

	Phase = PHASE_IDLE;
	PhaseStep = 0;
	Command = CMD_UNKNOWN;

	ST0_IC = 0;		// bit 7,6	Interrupt Code
	ST0_SE = 0;		// bit 5	Seek End
	ST0_EC = 0;		// bit 4	Equipment Check
	ST0_NR = 0;		// bit 3	Not Ready
	ST0_HD = 0;		// bit 2	Head Address
	ST0_DS = 0;		// bit 1,0	Drive Select

	ST1_EN = 0;		// bit 7	End of Cylinder
	ST1_DE = 0;		// bit 5	data Error
	ST1_OR = 0;		// bit 4	Over Run
	ST1_ND = 0;		// bit 2	No data
	ST1_NW = 0;		// bit 1	Not Writable
	ST1_MA = 0;		// bit 0	Missing Address Mark

	ST2_CM = 0;		// bit 6	Control Mark
	ST2_DD = 0;		// bit 5	data Error in data Field
	ST2_NC = 0;		// bit 4	No Cylinder
	ST2_SH = 0;		// bit 3	Scan Equal Satisfied
	ST2_SN = 0;		// bit 2	Scan Not Satisfied
	ST2_BC = 0;		// bit 1	Bad Cylinder
	ST2_MD = 0;		// bit 0	Missing Address Mark in data Field

	ST3_FLT = 0;		// bit 7	Fault
	ST3_WP  = 0;		// bit 6	Write Protect
	ST3_RDY = 0;		// bit 5	Ready
	ST3_TK0 = 0;		// bit 4	Track 0
	ST3_2S  = 0;		// bit 3	Two Side
	ST3_HD  = 0;		// bit 2	Head Address
	ST3_DS  = 0;		// bit 1,0	Drive Select

	PCN = 0;		// Present Cylinder Number
}

byte TC8566AF::makeST0() const
{
	return  (ST0_IC << 6) |		// bit 7,6	Interrupt Code
	        (ST0_SE << 5) |		// bit 5	Seek End
	        (ST0_EC << 4) |		// bit 4	Equipment Check
	        (ST0_NR << 3) |		// bit 3	Not Ready
	        (ST0_HD << 2) |		// bit 2	Head Address
	        (ST0_DS << 0);		// bit 1,0	Drive Select
}

byte TC8566AF::makeST1() const
{
	return  (ST1_EN << 7) |		// bit 7	End of Cylinder
	        (ST1_DE << 5) |		// bit 5	data Error
	        (ST1_OR << 4) |		// bit 4	Over Run
	        (ST1_ND << 2) |		// bit 2	No data
	        (ST1_NW << 1) |		// bit 1	Not Writable
	        (ST1_MA << 0);		// bit 0	Missing Address Mark
}

byte TC8566AF::makeST2() const
{
	return  (ST2_CM << 6) |		// bit 6	Control Mark
	        (ST2_DD << 5) |		// bit 5	data Error in data Field
	        (ST2_NC << 4) |		// bit 4	No Cylinder
	        (ST2_SH << 3) |		// bit 3	Scan Equal Satisfied
	        (ST2_SN << 2) |		// bit 2	Scan Not Satisfied
	        (ST2_BC << 1) |		// bit 1	Bad Cylinder
	        (ST2_MD << 0);		// bit 0	Missing Address Mark in data Field
}

byte TC8566AF::makeST3() const
{
	return  (ST3_FLT << 7) |	// bit 7	Fault
	        (ST3_WP  << 6) |	// bit 6	Write Protect
	        (ST3_RDY << 5) |	// bit 5	Ready
	        (ST3_TK0 << 4) |	// bit 4	Track 0
	        (ST3_2S  << 3) |	// bit 3	Two Side
	        (ST3_HD  << 2) |	// bit 2	Head Address
	        (ST3_DS  << 0);		// bit 1,0	Drive Select
}

byte TC8566AF::readReg(int reg, const EmuTime& time)
{
	byte result = 0xFF;
	switch (reg) {
	case 4: // Main Status Register
		if (delayTime.before(time)) {
			RequestForMaster = 1;
		}
		result = (RequestForMaster << 7) |
		         (dataInputOutput  << 6) |
		         (NonDMAMode       << 5) |
		         (FDCBusy          << 4) |
		         (FDD3Busy         << 3) |
		         (FDD2Busy         << 2) |
		         (FDD1Busy         << 1) |
		         (FDD0Busy         << 0);
		break;

	case 5: // data port
		switch (Phase) {
		case PHASE_DATATRANSFER:
			result = readDataTransferPhase();
			RequestForMaster = 0;
			delayTime.reset(time);
			delayTime += 15;
			break;
		case PHASE_RESULT:
			result = readDataResultPhase();
			break;
		}
		break;
	}
	return result;
}

byte TC8566AF::peekReg(int reg, const EmuTime& time)
{
	byte result = 0xFF;
	switch (reg) {
	case 5: // data port
		switch (Phase) {
		case PHASE_DATATRANSFER:
			result = peekDataTransferPhase();
			break;
		case PHASE_RESULT:
			result = peekDataResultPhase();
			break;
		}
		break;
	default:
		result = readReg(reg, time);
		break;
	}
	return result;
}

byte TC8566AF::readDataTransferPhase()
{
	byte result = 0xFF;
	switch (Command) {
	case CMD_READ_DATA:
		if (SectorByteCount > 0) {
			SectorByteCount--;
			result = Sector[SectorPtr++];
		}
		if (SectorByteCount == 0) {
			Phase = PHASE_RESULT;
			PhaseStep = 0;
		}
		break;
	}
	return result;
}

byte TC8566AF::peekDataTransferPhase() const
{
	byte result = 0xFF;
	switch (Command) {
	case CMD_READ_DATA:
		if (SectorByteCount > 0) {
			result = Sector[SectorPtr];
		}
		break;
	}
	return result;
}

byte TC8566AF::readDataResultPhase()
{
	byte result = 0xFF;
	switch (Command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		switch (PhaseStep++) {
		case 0:
			result = makeST0();
			break;
		case 1:
			result = makeST1();
			break;
		case 2:
			result = makeST2();
			break;
		case 3:
			result = StartCylinder;
			break;
		case 4:
			result = StartHead;
			break;
		case 5:
			result = StartRecord;
			break;
		case 6:
			result = StartN;
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			dataInputOutput = 0;
			Command = CMD_UNKNOWN;
			break;
		}
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		switch (PhaseStep++) {
		case 0:
			result = makeST0();
			break;
		case 1:
			result = PCN;
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			dataInputOutput = 0;
			Command = CMD_UNKNOWN;
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch	(PhaseStep++) {
		case 0:
			result = makeST3();
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			dataInputOutput = 0;
			Command = CMD_UNKNOWN;
			break;
		}
		break;
	}
	return result;
}

byte TC8566AF::peekDataResultPhase() const
{
	byte result = 0xFF;
	switch (Command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
	case CMD_FORMAT:
		switch (PhaseStep) {
		case 0:
			result = makeST0();
			break;
		case 1:
			result = makeST1();
			break;
		case 2:
			result = makeST2();
			break;
		case 3:
			result = StartCylinder;
			break;
		case 4:
			result = StartHead;
			break;
		case 5:
			result = StartRecord;
			break;
		case 6:
			result = StartN;
			break;
		}
		break;

	case CMD_SENSE_INTERRUPT_STATUS:
		switch (PhaseStep) {
		case 0:
			result = makeST0();
			break;
		case 1:
			result = PCN;
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch	(PhaseStep) {
		case 0:
			result = makeST3();
			break;
		}
		break;
	}
	return result;
}


void TC8566AF::writeReg(int reg, byte data, const EmuTime& time)
{
	switch (reg) {
	case 2: // Control register 0
		drive[3]->setMotor((data & 0x80) != 0, time);
		drive[2]->setMotor((data & 0x40) != 0, time);
		drive[1]->setMotor((data & 0x20) != 0, time);
		drive[0]->setMotor((data & 0x10) != 0, time);
		EnableIntDma = (data & 0x08) != 0;
		NotReset     = (data & 0x04) != 0;
		DriveSelect  = (data & 3);	// drive select: 0 - 3
		break;

	case 3: // Control register 1
		ControlRegister1 = data;
		break;

	case 5: // data port
		switch (Phase) {
		case PHASE_IDLE:
			writeDataIdlePhase(data, time);
			break;

		case PHASE_COMMAND:
			writeDataCommandPhase(data, time);
			break;

		case PHASE_DATATRANSFER:
			writeDataTransferPhase(data, time);
			RequestForMaster = 0;
			delayTime.reset(time);
			delayTime += 15;
			break;

		case PHASE_RESULT:
			// nothing
			break;
		}
		break;
	}
}

void TC8566AF::writeDataIdlePhase(byte data, const EmuTime& /*time*/)
{
	Command = CMD_UNKNOWN;
	if ((data & 0x1f) == 0x06) Command = CMD_READ_DATA;
	if ((data & 0x3f) == 0x05) Command = CMD_WRITE_DATA;
	if ((data & 0x3f) == 0x09) Command = CMD_WRITE_DELETED_DATA;
	if ((data & 0x1f) == 0x0c) Command = CMD_READ_DELETED_DATA;
	if ((data & 0xbf) == 0x02) Command = CMD_READ_DIAGNOSTIC;
	if ((data & 0xbf) == 0x0a) Command = CMD_READ_ID;
	if ((data & 0xbf) == 0x0d) Command = CMD_FORMAT;
	if ((data & 0x1f) == 0x11) Command = CMD_SCAN_EQUAL;
	if ((data & 0x1f) == 0x19) Command = CMD_SCAN_LOW_OR_EQUAL;
	if ((data & 0x1f) == 0x1d) Command = CMD_SCAN_HIGH_OR_EQUAL;
	if ((data & 0xff) == 0x0f) Command = CMD_SEEK;
	if ((data & 0xff) == 0x07) Command = CMD_RECALIBRATE;
	if ((data & 0xff) == 0x08) Command = CMD_SENSE_INTERRUPT_STATUS;
	if ((data & 0xff) == 0x03) Command = CMD_SPECIFY;
	if ((data & 0xff) == 0x04) Command = CMD_SENSE_DEVICE_STATUS;

	Phase = PHASE_COMMAND;
	PhaseStep = 0;
	FDCBusy = 1;

	switch (Command) {
		case CMD_READ_DATA:
		case CMD_WRITE_DATA:
		case CMD_FORMAT:
			ST0_IC = 0;
			ST1_ND = 0;
			MT  = (data & 0x80) != 0;
			MFM = (data & 0x40) != 0;
			SK  = (data & 0x20) != 0;
			break;

		case CMD_RECALIBRATE:
			ST0_SE = 0;	// Seek End = 0
			break;

		case CMD_SENSE_INTERRUPT_STATUS:
			Phase = PHASE_RESULT;
			dataInputOutput = 1;
			break;

		case CMD_SEEK:
		case CMD_SPECIFY:
		case CMD_SENSE_DEVICE_STATUS:
			break;

		default:
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			Command = CMD_UNKNOWN;
			break;
	}

}

void TC8566AF::writeDataCommandPhase(byte data, const EmuTime& time)
{
	switch (Command) {
	case CMD_READ_DATA:
	case CMD_WRITE_DATA:
		switch (PhaseStep++) {
		case 0:
			ST0_DS = data & 3; // Copy Drive Select
			ST3_DS = data & 3; // Copy Drive Select
			ST3_RDY = drive[DriveSelect]->ready()          ? 1 : 0;
			ST3_WP  = drive[DriveSelect]->writeProtected() ? 1 : 0;
			ST3_TK0 = drive[DriveSelect]->track00(time)    ? 1 : 0;
			ST3_HD  = drive[DriveSelect]->doubleSided()    ? 1 : 0;
			if (drive[DriveSelect]->dummyDrive()) {
				ST0_IC = 1; // bit 7,6 Interrupt Code
			}
			// HS
			break;
		case 1:
			StartCylinder = data;
			break;
		case 2:
			StartHead = data;
			break;
		case 3:
			StartRecord = data;
			break;
		case 4:
			StartN = data;
			SectorByteCount = ((MT == 0) && (MFM == 1) && (StartN == 2)) ? 512 : 0;
			break;
		case 5: // EOT: End Of Track
			break;
		case 6: // GPL: Gap Length
			break;
		case 7: // DTL: DATA Length
			if (Command == CMD_READ_DATA) {
				// read
				try {
					byte dummy;
					int dummy2;
					drive[DriveSelect]->setSide(StartHead);
					drive[DriveSelect]->read(
					    StartRecord, Sector, dummy,
					    dummy, dummy, dummy2);
				} catch (MSXException& e) {
					ST0_IC = 1;
					ST1_ND = 1;
				}
				dataInputOutput = 1;
			} else {
				// write
				dataInputOutput = 0;
			}
			Phase = PHASE_DATATRANSFER;
			PhaseStep = 0;
			SectorPtr = 0;
			break;
		}
		break;

	case CMD_FORMAT:
		switch (PhaseStep++) {
		case 0:
			ST0_DS = data & 3; // Copy Drive Select
			ST3_DS = data & 3; // Copy Drive Select
			ST3_RDY = drive[DriveSelect]->ready()          ? 1 : 0;
			ST3_WP  = drive[DriveSelect]->writeProtected() ? 1 : 0;
			ST3_TK0 = drive[DriveSelect]->track00(time)    ? 1 : 0;
			ST3_HD  = drive[DriveSelect]->doubleSided()    ? 1 : 0;
			if (drive[DriveSelect]->dummyDrive()) {
				ST0_IC = 1; // bit 7,6 Interrupt Code
			}
			break;
		case 1:
			StartN = data;
			break;
		case 2:
			SectorsPerCylinder = data;
			StartRecord        = data;
			break;
		case 4:
			SectorPtr       = 0;
			dataInputOutput = 0;
			Phase           = PHASE_DATATRANSFER;
			PhaseStep       = 0;
			break;
		}
		break;

	case CMD_SEEK:
		switch (PhaseStep++) {
		case 0:
			ST0_DS = data & 3; // Copy Drive Select
			ST3_DS = data & 3; // Copy Drive Select
			ST3_RDY = drive[DriveSelect]->ready()          ? 1 : 0;
			ST3_WP  = drive[DriveSelect]->writeProtected() ? 1 : 0;
			ST3_TK0 = drive[DriveSelect]->track00(time)    ? 1 : 0;
			ST3_HD  = drive[DriveSelect]->doubleSided()    ? 1 : 0;
			if (drive[DriveSelect]->dummyDrive()) {
				ST0_IC = 1; // bit 7,6 Interrupt Code
			}
			break;
		case 1: {
			int maxSteps = 255;
			while (data > PCN && maxSteps--) {
				drive[DriveSelect]->step(true, time);
				PCN++;
			}
			while (data < PCN && maxSteps--) {
				drive[DriveSelect]->step(false, time);
				PCN--;
			}
			ST0_SE = 1;	// Seek End = 1
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			Command = CMD_UNKNOWN;
			PRT_DEBUG("FDC: PCN " << (int)PCN);
			break;
		}
		}
		break;

	case CMD_RECALIBRATE:
		switch (PhaseStep++) {
		case 0: {
			ST0_DS = data & 3; // Copy Drive Select
			ST3_DS = data & 3; // Copy Drive Select
			ST3_RDY = drive[DriveSelect]->ready()          ? 1 : 0;
			ST3_WP  = drive[DriveSelect]->writeProtected() ? 1 : 0;
			ST3_TK0 = drive[DriveSelect]->track00(time)    ? 1 : 0;
			ST3_HD  = drive[DriveSelect]->doubleSided()    ? 1 : 0;
			if (drive[DriveSelect]->dummyDrive()) {
				ST0_IC = 1; // bit 7,6 Interrupt Code
			}
			int maxSteps = 255;
			while (!drive[DriveSelect]->track00(time) && maxSteps--) {
				drive[DriveSelect]->step(false, time);
				PCN--;
			}
			ST0_SE = 1;	// Seek End = 1
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			Command = CMD_UNKNOWN;
			PRT_DEBUG("FDC: PCN " << (int)PCN);
			break;
		}
		}
		break;

	case CMD_SPECIFY:
		switch (PhaseStep++) {
		case 0:
			break;
		case 1:
			FDCBusy = 0;
			Phase = PHASE_IDLE;
			Command = CMD_UNKNOWN;
			break;
		}
		break;

	case CMD_SENSE_DEVICE_STATUS:
		switch (PhaseStep++) {
		case 0:
			ST0_DS = data & 3; // Copy Drive Select
			ST3_DS = data & 3; // Copy Drive Select
			ST3_RDY = drive[DriveSelect]->ready()          ? 1 : 0;
			ST3_WP  = drive[DriveSelect]->writeProtected() ? 1 : 0;
			ST3_TK0 = drive[DriveSelect]->track00(time)    ? 1 : 0;
			ST3_HD  = drive[DriveSelect]->doubleSided()    ? 1 : 0;
			if (drive[DriveSelect]->dummyDrive()) {
				ST0_IC = 1; // bit 7,6 Interrupt Code
			}
			Phase = PHASE_RESULT;
			PhaseStep = 0;
			dataInputOutput = 1;
			break;
		}
		break;
	}
}

void TC8566AF::writeDataTransferPhase(byte data, const EmuTime& /*time*/)
{
	switch (Command) {
	case CMD_WRITE_DATA:
		if (SectorByteCount > 0) {
			SectorByteCount--;
			Sector[SectorPtr++] = data;
		}
		if (SectorByteCount == 0) {
			try {
				byte dummy;
				int dummy2;
				drive[DriveSelect]->setSide(StartHead);
				drive[DriveSelect]->write(
				    StartRecord, Sector, dummy,
				    dummy, dummy, dummy2);
				Phase = PHASE_RESULT;
				PhaseStep = 0;
				dataInputOutput = 1;
			} catch (MSXException& e) {
			}
		}
		break;

	case CMD_FORMAT:
		switch(PhaseStep & 3) {
		case 0:
			PCN = data;
			break;
		case 1:
			try {
				memset(Sector, 0, 512);
				byte dummy;
				int dummy2;
				drive[DriveSelect]->setSide(data);
				drive[DriveSelect]->write(
				    StartRecord, Sector, dummy,
				    dummy, dummy, dummy2);
			} catch (MSXException& e) {
			}
			break;
		case 2:
			StartRecord = data;
			break;
		}

		if (++PhaseStep == (4 * SectorsPerCylinder - 2)) {
			Phase       = PHASE_RESULT;
			PhaseStep   = 0;
			dataInputOutput = 1;
		}
		break;
	}
}

bool TC8566AF::diskChanged(int driveno)
{
	return drive[driveno]->diskChanged();
}

bool TC8566AF::peekDiskChanged(int driveno) const
{
	return drive[driveno]->peekDiskChanged();
}

} // namespace openmsx
