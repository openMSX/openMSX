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

	virtual void reset();
	virtual byte getStatusReg(const EmuTime &time);
	virtual byte getTrackReg(const EmuTime &time);
	virtual byte getSectorReg(const EmuTime &time);
	virtual byte getDataReg(const EmuTime &time);
	virtual void setCommandReg(byte value,const EmuTime &time);
	virtual void setTrackReg(byte value,const EmuTime &time);
	virtual void setSectorReg(byte value,const EmuTime &time);
	virtual void setDataReg(byte value,const EmuTime &time);
	virtual byte getSideSelect(const EmuTime &time);
	virtual byte getDriveSelect(const EmuTime &time);
	virtual byte getMotor(const EmuTime &time);
	virtual byte getIRQ(const EmuTime &time);
	virtual byte getDTRQ(const EmuTime &time);
	virtual void setSideSelect(byte value,const EmuTime &time);
	virtual void setDriveSelect(byte value,const EmuTime &time);
	virtual void setMotor(byte value,const EmuTime &time);
	
  private:
	byte timePerStep[4]; // {3,6,10,15} in ms case of of 2 MHz clock, double this if a 1MHz clock is used! (MSX=1MHz clock :-)
	
	/*
	EmuTime commandStart;
	EmuTime commandEnd;
	*/
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
	
	//flags could have been bools ofcourse
	//Names taken from Table 2 in the WD279X-02 specs pdf
	byte Vflag;		//Track Number Verify Flag
	byte hflag;		//Head Load Flag
	byte Tflag;		//Track Update Flag
	byte Dflag;		//Data Address Mark Flag
	byte Cflag;		//Side Compare Flag
	byte mflag;		//Multi Record Flag
	byte Eflag;		//15 MS delay
	byte Sflag;		//Side Compare Flag2
	
	
	bool directionIn;
	bool INTRQ;
	bool DRQ;

	byte dataBuffer[1024];	// max sector size possible
	int dataCurrent;	// which byte in dataBuffer is next to be read/write
	int dataAvailable;	// how many bytes left in sector
};

#endif
