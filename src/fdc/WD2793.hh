// $Id$

#ifndef __WD2793_HH__
#define __WD2793_HH__

#include "FDC.hh"

// forward declarations
class FDCBackEnd;


class WD2793 : public FDC
{
	public:
		WD2793(MSXConfig::Device *config);
		virtual ~WD2793();

		void reset();
		byte getStatusReg(const EmuTime &time);
		byte getTrackReg(const EmuTime &time);
		byte getSectorReg(const EmuTime &time);
		byte getDataReg(const EmuTime &time);
		void setCommandReg(byte value,const EmuTime &time);
		void setTrackReg(byte value,const EmuTime &time);
		void setSectorReg(byte value,const EmuTime &time);
		void setDataReg(byte value,const EmuTime &time);
		byte getSideSelect(const EmuTime &time);
		byte getDriveSelect(const EmuTime &time);
		byte getMotor(const EmuTime &time);
		byte getIRQ(const EmuTime &time);
		byte getDTRQ(const EmuTime &time);
		void setSideSelect(byte value,const EmuTime &time);
		void setDriveSelect(byte value,const EmuTime &time);
		void setMotor(byte value,const EmuTime &time);

	private:
		byte timePerStep[4];	// {3,6,10,15} in ms case of of 2 MHz clock 
					// double this if a 1MHz clock is used!
					// (MSX=1MHz clock :-)

		// EmuTime commandStart;
		// EmuTime commandEnd;
		EmuTimeFreq<1000> commandEnd;
		EmuTimeFreq<1000> motorStartTime[2];


		byte statusReg;
		byte commandReg;
		byte sectorReg;
		byte trackReg;
		byte dataReg;

		byte current_drive;
		byte motor_drive[4]; //Brazilian based machines support up to 4 drives per FDC

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
