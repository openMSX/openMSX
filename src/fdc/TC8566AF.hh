// $Id$

/**
 * Based on code from NLMSX written by Frits Hilderink
 */

#ifndef __TC8566AF_HH__
#define __TC8566AF_HH__

#include "openmsx.hh"

namespace openmsx {

class EmuTime;
class DiskDrive;


class TC8566AF
{
	public:
		TC8566AF(DiskDrive* drive[4], const EmuTime &time);
		virtual ~TC8566AF();
		
		void reset(const EmuTime &time);
		byte readReg(int reg, const EmuTime &time);
		void writeReg(int reg, byte value, const EmuTime &time);

	private:
		DiskDrive* drive[4];

		byte makeST0();
		byte makeST1();
		byte makeST2();
		byte makeST3(const EmuTime &time);

		// Control register 0
		byte EnableIntDma;	// always 0
		byte NotReset;
		byte DriveSelect;	// drive select: 0 - 3

		// Control register 1
		byte ControlRegister1;

		// Main Status Register
		byte RequestForMaster;
		byte dataInputOutput;	// 1 = read from data register to CPU
					// 0 = write from CPU to data register
		byte NonDMAMode;	// 1 = Non DMA Mode
		byte FDCBusy;		// Execution/Command/Result phase
		byte FDD3Busy;
		byte FDD2Busy;
		byte FDD1Busy;
		byte FDD0Busy;

		byte Phase;		// 0 = no phase
					// 1 = Command
					// 2 = Execution
					// 3 = Result
		byte PhaseStep;

		byte Command;		// 0 = No Command Active
					// 1 = Read Data Command
					// 2 = Write Data Command
					// 3 = Write Deleted Data Command
					// 4 = Read Deleted Data Command
					// 5 = Read Diagnostic Command
					// 6 = Read ID Command
					// 7 = Format Command
					// 8 = Scan Equal Command
					// 9 = Scan Low or Equal Command
					// 10 = Scan High or Equal Command
					// 11 = Seek Command
					// 12 = Recalibrate Command
					// 13 = Sense Interrupt Status Command
					// 14 = Specify Command
					// 15 = Sense Device Status Command
					// 16 = Invalid Command

		byte ST0_IC;		// bit 7,6	Interrupt Code
		byte ST0_SE;		// bit 5	Seek End
		byte ST0_EC;		// bit 4	Equipment Check
		byte ST0_NR;		// bit 3	Not Ready
		byte ST0_HD;		// bit 2	Head Address
		byte ST0_DS;		// bit 1,0	Drive Select

		byte ST1_EN;		// bit 7	End of Cylinder
		byte ST1_DE;		// bit 5	Data Error
		byte ST1_OR;		// bit 4	Over Run
		byte ST1_ND;		// bit 2	No Data
		byte ST1_NW;		// bit 1	Not Writable
		byte ST1_MA;		// bit 0	Missing Address Mark

		byte ST2_CM;		// bit 6	Control Mark
		byte ST2_DD;		// bit 5	Data Error in Data Field
		byte ST2_NC;		// bit 4	No Cylinder
		byte ST2_SH;		// bit 3	Scan Equal Satisfied
		byte ST2_SN;		// bit 2	Scan Not Satisfied
		byte ST2_BC;		// bit 1	Bad Cylinder
		byte ST2_MD;		// bit 0	Missing Address Mark in Data Field

		byte ST3_FLT;		// bit 7	Fault
		byte ST3_WP;		// bit 6	Write Protect
		byte ST3_RDY;		// bit 5	Ready
		byte ST3_TK0;		// bit 4	Track 0
		byte ST3_2S;		// bit 3	Two Side
		byte ST3_HD;		// bit 2	Head Address
		byte ST3_DS;		// bit 1,0	Drive Select

		byte PCN;		// Present Cylinder Number

		byte MT;		// bit 7
		byte MFM;		// bit 6
		byte SK;		// bit 5
		byte StartCylinder;
		byte StartHead;
		byte StartRecord;
		byte StartN;

	//	byte DiskLEDOn;		// 0 = disk LED off,	1 = disk LED on
	//	byte DiskMotorOn;	// 0 = disk motor off,	1 = disk motor on
	//	byte DiskSelect;	// 0 = disk 0,			1 = disk 1
	//	byte SideSelect;	// 0 = side 0,			1 = side 1

	//	int LastStepDirection;	// Default = 1

	//	byte dataReg;
	//	byte TrackReg;
	//	byte SectorReg;
	//	byte CommandReg;

	//	byte StatusReg;
	//	byte StatusContext;	// 0 = All type I commands
					// 1 = during read address
					// 2 = during read sector
					// 3 = during read track
					// 4 = during write sector
					// 5 = during write track

	//	byte FlagNotReady;	// bit 7
	//	byte FlagWriteProtect;	// bit 6

	//	byte FlagRecordType;	// bit 5
	//	byte FlagHeadLoaded;	// bit 5

	//	byte FlagSeekError;	// bit 4
	//	byte FlagRecordNotFound;// bit 4

	//	byte FlagCRCError;	// bit 3

	//	byte FlagTrack0;	// bit 2
	//	byte FlagLostData;	// bit 2

	//	byte FlagIndexPulse;	// bit 1
	//	byte FlagDataRequest;	// bit 1

	//	byte FlagBusy;		// bit 0

	//	byte InterruptRequest;

		word SectorPtr;
		byte Sector[512];
		word SectorByteCount;
};

} // namespace openmsx

#endif
