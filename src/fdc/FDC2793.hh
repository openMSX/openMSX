// $Id$

#ifndef __FDC2793_HH__
#define __FDC2793_HH__

#include "FDC.hh"


class FDC2793 : public FDC
{
  public:
	~FDC2793();

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
    byte getIRQ(const EmuTime &time);
    byte getDTRQ(const EmuTime &time);
    void setSideSelect(byte value,const EmuTime &time);
    void setDriveSelect(byte value,const EmuTime &time);

	FDC2793(MSXConfig::Device *config);
  private:
    
	byte timePerStep[4]; // {3,6,10,15} in ms case of of 2 MHz clock, double this if a 1MHz clock is used! (MSX=1MHz clock :-)
	
  	std::string driveName1;

	/*
	EmuTime commandStart;
	EmuTime commandEnd;
	*/
	EmuTimeFreq<1000> commandEnd;


	byte statusReg;
	byte commandReg;
	byte sectorReg;
	byte trackReg;
	byte dataReg;

	byte driveReg; //bestaat niet in de FDC zelf maar als externe logica
	byte current_drive;
	byte motor_drive;

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
