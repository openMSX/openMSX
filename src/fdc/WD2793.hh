// $Id$

#ifndef __WD2793_HH__
#define __WD2793_HH__

#include "FDC.hh"

// forward declarations
class FDCBackEnd;


class WD2793 : public FDC
{
	public: 
		// Brazilian based machines support up to 4 drives per FDC
		enum DriveNum {
			DRIVE_A = 0,
			DRIVE_B = 1,
			DRIVE_C = 2,
			DRIVE_D = 3,
			NUM_DRIVES = 4,
			NO_DRIVE = 255
		};
	
		WD2793(MSXConfig::Device *config, const EmuTime &time);
		~WD2793();

		void reset(const EmuTime &time);
		
		byte getStatusReg(const EmuTime &time);
		byte getTrackReg (const EmuTime &time);
		byte getSectorReg(const EmuTime &time);
		byte getDataReg  (const EmuTime &time);
		
		void setCommandReg(byte value, const EmuTime &time);
		void setTrackReg  (byte value, const EmuTime &time);
		void setSectorReg (byte value, const EmuTime &time);
		void setDataReg   (byte value, const EmuTime &time);
		
		bool getSideSelect(const EmuTime &time);
		void setSideSelect(bool side, const EmuTime &time);
		
		DriveNum getDriveSelect(const EmuTime &time);
		void setDriveSelect(DriveNum drive, const EmuTime &time);
		
		bool getMotor(const EmuTime &time);
		void setMotor(bool status, const EmuTime &time);
		
		bool getIRQ(const EmuTime &time);
		bool getDTRQ(const EmuTime &time);

	private:
		static const byte timePerStep[4];

		// EmuTime commandStart;
		// EmuTime commandEnd;
		EmuTimeFreq<1000000> commandEnd;
		EmuTimeFreq<1000000> motorStartTime[NUM_DRIVES];
		EmuTimeFreq<1000000> DRQTime[NUM_DRIVES];

		byte statusReg;
		byte commandReg;
		byte sectorReg;
		byte trackReg;
		byte dataReg;

		DriveNum current_drive;
		bool motorStatus[NUM_DRIVES];

		byte current_track;
		byte current_sector;
		byte current_side;
		byte stepSpeed;

		//Names taken from Table 2 in the WD279X-02 specs pdf
		bool Vflag;	//Track Number Verify Flag
		bool hflag;	//Head Load Flag
		bool Tflag;	//Track Update Flag
		bool Dflag;	//Data Address Mark Flag
		bool Cflag;	//Side Compare Flag
		bool mflag;	//Multi Record Flag
		bool Eflag;	//15 MS delay
		bool Sflag;	//Side Compare Flag2

		bool directionIn;
		bool INTRQ;
		bool DRQ;

		byte dataBuffer[1024];	// max sector size possible
		int dataCurrent;	// which byte in dataBuffer is next to be read/write
		int dataAvailable;	// how many bytes left in sector
};

#endif
