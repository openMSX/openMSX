// $Id$

/**
 * Based on code from NLMSX written by Frits Hilderink
 */

#include "TC8566AF.hh"
#include "DiskDrive.hh"


TC8566AF::TC8566AF(DiskDrive* drive[4], const EmuTime &time)
{
	this->drive[0] = drive[0];
	this->drive[1] = drive[1];
	this->drive[2] = drive[2];
	this->drive[3] = drive[3];
	reset(time);
}

TC8566AF::~TC8566AF()
{
}

void TC8566AF::reset(const EmuTime &time)
{
	// Control register 0
	MotorEnable3 = 0;
	MotorEnable2 = 0;
	MotorEnable1 = 0;
	MotorEnable0 = 0;
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

	Phase = 0;
	PhaseStep = 0;
	Command = 0;

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
	ST3_RDY = 1;		// bit 5	Ready
	ST3_TK0 = 0;		// bit 4	Track 0
	ST3_2S  = 0;		// bit 3	Two Side
	ST3_HD  = 0;		// bit 2	Head Address
	ST3_DS  = 0;		// bit 1,0	Drive Select

	PCN = 0;		// Present Cylinder Number
}

byte TC8566AF::makeST0()
{
	return  (ST0_IC << 6) |		// bit 7,6	Interrupt Code
		(ST0_SE << 5) |		// bit 5	Seek End
		(ST0_EC << 4) |		// bit 4	Equipment Check
		(ST0_NR << 3) |		// bit 3	Not Ready
		(ST0_HD << 2) |		// bit 2	Head Address
		(ST0_DS);		// bit 1,0	Drive Select
}

byte TC8566AF::makeST1()
{
	return  (ST1_EN << 7) |		// bit 7	End of Cylinder
		(ST1_DE << 5) |		// bit 5	data Error
		(ST1_OR << 4) |		// bit 4	Over Run
		(ST1_ND << 2) |		// bit 2	No data
		(ST1_NW << 1) |		// bit 1	Not Writable
		(ST1_MA);		// bit 0	Missing Address Mark
}

byte TC8566AF::makeST2()
{
	return  (ST2_CM << 6) |		// bit 6	Control Mark
		(ST2_DD << 5) |		// bit 5	data Error in data Field
		(ST2_NC << 4) |		// bit 4	No Cylinder
		(ST2_SH << 3) |		// bit 3	Scan Equal Satisfied
		(ST2_SN << 2) |		// bit 2	Scan Not Satisfied
		(ST2_BC << 1) |		// bit 1	Bad Cylinder
		(ST2_MD);		// bit 0	Missing Address Mark in data Field
}

byte TC8566AF::makeST3()
{
/*
	switch (MSXDiskPresent(TC8566AFGetDiskCtx())) {
	case 0:
		ST3_RDY = 0;
		ST3_WP  = 1;
		break;
	case 1:
		ST3_RDY = 1;
		ST3_WP  = 0;
		break;
	case 2:
		ST3_RDY = 1;
		ST3_WP  = 1;
		break;
	}
*/
	return  (ST3_FLT << 7) |	// bit 7	Fault
		(ST3_WP  << 6) |	// bit 6	Write Protect
		(ST3_RDY << 5) |	// bit 5	Ready
		((PCN == 0) << 4) |// (ST3_TK0 << 4) |	// bit 4	Track 0
		(ST3_2S  << 3) |	// bit 3	Two Side
		(ST3_HD  << 2) |	// bit 2	Head Address
		(ST3_DS);		// bit 1,0	Drive Select
}

byte TC8566AF::readReg(int reg)
{
	byte	Value;

	switch	(reg) {
	case 4: // Main Status Register
		Value = (RequestForMaster << 7) |
			(dataInputOutput  << 6) |
			(NonDMAMode       << 5) |
			(FDCBusy          << 4) |
			(FDD3Busy         << 3) |
			(FDD2Busy         << 2) |
			(FDD1Busy         << 1) |
			(FDD0Busy);
		break;

	case 5: // data port
		switch (Command) {
		case 1: // Read data Command
			switch (Phase) {
			case 2: // Execution Phase
				if (SectorByteCount > 0) {
					SectorByteCount--;
					Value = Sector[SectorPtr++];
				}
				if (SectorByteCount == 0) {
					Phase++;
					PhaseStep = 0;
				}
				break;
			case 3: // Result Phase
				switch	(PhaseStep++) {
				case 0:
					Value = makeST0();
					break;
				case 1:
					Value = makeST1();
					break;
				case 2:
					Value = makeST2();
					break;
				case 3:
					Value = StartCylinder;
					break;
				case 4:
					Value = StartHead;
					break;
				case 5:
					Value = StartRecord;
					break;
				case 6:
					Value = StartN;
					FDCBusy = 0;
					Phase = 0;
					dataInputOutput = 0;
					Command = 0;
					break;
				}
			}
			break;
		case 2: // Write data Command
			switch (Phase) {
			case 3: // Result Phase
				switch (PhaseStep++) {
				case 0:
					Value = makeST0();
					break;
				case 1:
					Value = makeST1();
					break;
				case 2:
					Value = makeST2();
					break;
				case 3:
					Value = StartCylinder;
					break;
				case 4:
					Value = StartHead;
					break;
				case 5:
					Value = StartRecord;
					break;
				case 6:
					Value = StartN;
					FDCBusy = 0;
					Phase = 0;
					dataInputOutput = 0;
					Command = 0;
					break;
				}
			}
			break;
		case 13:// Sense Interrupt Status Command
			// Result Phase
			switch (PhaseStep++) {
			case 0:
				Value = makeST0();
				break;
			case 1:
				Value = PCN;
				FDCBusy = 0;
				Phase = 0;
				dataInputOutput = 0;
				Command = 0;
				break;
			}
			break;
		case 15:// Sense Device Status Command
			// Result Phase
			switch	(PhaseStep++) {
			case 0:
				Value = makeST3();
				FDCBusy = 0;
				Phase = 0;
				dataInputOutput = 0;
				Command = 0;
				break;
			}
			break;
		default:
			break;
		}
		break;
	}
	return Value;
}

void TC8566AF::writeReg(int reg, byte data)
{
	switch (reg) {
	case 2: // Control register 0
		MotorEnable3 = (data & 0x80) != 0;
		MotorEnable2 = (data & 0x40) != 0;
		MotorEnable1 = (data & 0x20) != 0;
		MotorEnable0 = (data & 0x10) != 0;
		EnableIntDma = (data & 0x08) != 0;
		NotReset     = (data & 0x04) != 0;
		DriveSelect  = (data & 3);	// drive select: 0 - 3
		break;

	case 3: // Control register 1
		ControlRegister1 = data;
		break;

	case 5: // data port
		switch (Phase) {
		case 0:
			Command = 16; // 16 = Invalid Command
			if ((data & 0x1f) == 0x06) Command = 1; // 1 = Read data Command
			if ((data & 0x3f) == 0x05) Command = 2; // 2 = Write data Command
			if ((data & 0x3f) == 0x09) Command = 3; // 3 = Write Deleted data Command
			if ((data & 0x1f) == 0x0c) Command = 4; // 4 = Read Deleted data Command
			if ((data & 0xbf) == 0x02) Command = 5; // 5 = Read Diagnostic Command
			if ((data & 0xbf) == 0x0a) Command = 6; // 6 = Read ID Command
			if ((data & 0xbf) == 0x0d) Command = 7; // 7 = Format Command
			if ((data & 0x1f) == 0x11) Command = 8; // 8 = Scan Equal Command
			if ((data & 0x1f) == 0x19) Command = 9; // 9 = Scan Low or Equal Command
			if ((data & 0x1f) == 0x1d) Command = 10;// 10 = Scan High or Equal Command
			if ((data & 0xff) == 0x0f) Command = 11;// 11 = Seek Command
			if ((data & 0xff) == 0x07) Command = 12;// 12 = Recalibrate Command
			if ((data & 0xff) == 0x08) Command = 13;// 13 = Sense Interrupt Status Command
			if ((data & 0xff) == 0x03) Command = 14;// 14 = Specify Command
			if ((data & 0xff) == 0x04) Command = 15;// 15 = Sense Device Status Command

			Phase++;
			PhaseStep = 0;
			FDCBusy = 1;

		case 1: // Command Phase
			switch (Command) {
			case 1: // Read data Command
				switch (PhaseStep++) {
				case 0:
					ST0_IC = 0;
					ST1_ND = 0;
					MT  = (data & 0x80) != 0;
					MFM = (data & 0x40) != 0;
					SK  = (data & 0x20) != 0;
					break;
				case 1:
					ST0_DS = data & 3; // Copy Drive Select
					ST3_DS = data & 3; // Copy Drive Select
					// HS
					break;
				case 2:
					StartCylinder = data;
					break;
				case 3:
					StartHead = data;
					break;
				case 4:
					StartRecord = data;
					break;
				case 5:
					StartN = data;
					if ((MT == 0) && (MFM == 1) && (StartN == 2))
						SectorByteCount = 512;
					else
						SectorByteCount = 0;
					break;
				case 6: // EOT: End Of Track
					break;
				case 7: // GPL: Gap Length
					break;
				case 8:
					// DTL: DATA Length
					try {
						drive[DriveSelect]->read(
						    StartRecord, 512, Sector);
						Phase++;
						PhaseStep = 0;
						dataInputOutput = 1;
						SectorPtr = 0;
					} catch (MSXException &e) {
						Phase++;
						PhaseStep = 0;
						dataInputOutput = 1;
						SectorPtr = 0;
						ST0_IC = 1;
						ST1_ND = 1;
					}
					break;
				}
				break;
			case 2: // Write data Command
				// Command Phase
				switch (PhaseStep++) {
				case 0:
					ST0_IC = 0;
					ST1_ND = 0;
					MT  = (data & 0x80) != 0;
					MFM = (data & 0x40) != 0;
					SK  = (data & 0x20) != 0;
					break;
				case 1:
					ST0_DS = data & 3; // Copy Drive Select
					ST3_DS = data & 3; // Copy Drive Select
					// HS
					break;
				case 2:
					StartCylinder = data;
					break;
				case 3:
					StartHead = data;
					break;
				case 4:
					StartRecord = data;
					break;
				case 5:
					StartN = data;
					if ((MT == 0) && (MFM == 1) && (StartN == 2))
						SectorByteCount = 512;
					else
						SectorByteCount = 0;
					break;
				case 6: // EOT: End Of Track
					break;
				case 7: // GPL: Gap Length
					break;
				case 8: // DTL: DATA Length
					Phase++;
					PhaseStep = 0;
					dataInputOutput = 0;
					SectorPtr = 0;
					break;
				}
				break;
			case 11:// Seek Command
				switch (PhaseStep++) {
				case 0:
					break;
				case 1:
					ST0_DS = data & 3; // Copy Drive Select
					ST3_DS = data & 3; // Copy Drive Select
					break;
				case 2:
					PCN = data;
					ST0_SE = 1;	// Seek End = 1
					FDCBusy = 0;
					Phase = 0;
					Command = 0;
					break;
				}
				break;
			case 12:// Recalibrate Command
				switch (PhaseStep++) {
				case 0:
					ST0_SE = 0;	// Seek End = 0
					break;
				case 1:
					ST0_DS = data & 3; // Copy Drive Select
					ST3_DS = data & 3; // Copy Drive Select
					PCN = 0;
					ST0_SE = 1;	// Seek End = 1
					FDCBusy = 0;
					Phase = 0;
					Command = 0;
					break;
				}
				break;
			case 13:// Sense Interrupt Status Command
				Phase = 2;
				dataInputOutput = 1;
				break;
			case 14:// Specify Command
				switch (PhaseStep++) {
				case 0:
					break;
				case 1:
					break;
				case 2:
					FDCBusy = 0;
					Phase = 0;
					Command = 0;
					break;
				}
				break;
			case 15:// Sense Device Status Command
				switch (PhaseStep++) {
				case 0:
					break;
				case 1:
					ST0_DS = data & 3; // Copy Drive Select
					ST3_DS = data & 3; // Copy Drive Select
					Phase = 2;
					PhaseStep = 0;
					dataInputOutput = 1;
					break;
				}
				break;
			default:
				FDCBusy = 0;
				Phase = 0;
				Command = 0;
				break;
			}
			break;
		case 2: // Execution Phase
			switch (Command) {
			case 2: // Write data Command
				if (SectorByteCount > 0) {
					SectorByteCount--;
					Sector[SectorPtr++] = data;
				}
				if (SectorByteCount == 0) {
					try {
						drive[DriveSelect]->write(
						    StartRecord, 512, Sector);
						Phase++;
						PhaseStep = 0;
						dataInputOutput = 1;
					} catch (MSXException &e) {
					}
				}
				break;
			}
			break;
		case 3: // Result
			break;
		}
		break;
	}
}
