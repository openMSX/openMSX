#ifndef __FDC2793_HH__
#define __FDC2793_HH__

#include "FDC.hh"

class FDCBackEnd;

class FDC2793 : public FDC
{
  public:
	virtual ~FDC2793();

	virtual void reset();
	//virtual byte getStatusReg(const EmuTime &time);
	virtual byte getTrackReg(const EmuTime &time);
	virtual byte getSectorReg(const EmuTime &time);
	virtual byte getDataReg(const EmuTime &time);
	//virtual void setCommandReg(byte value,const EmuTime &time);
	virtual void setTrackReg(byte value,const EmuTime &time);
	virtual void setSectorReg(byte value,const EmuTime &time);
	//virtual void setDataReg(byte value,const EmuTime &time);

	FDC2793(MSXConfig::Device *config);
  private:
  	FDCBackEnd *backend;

	byte statusReg;
	byte commandReg;
	byte sectorReg;
	byte trackReg;
	byte dataReg;

	byte current_track;
	byte current_sector;
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
};
#endif
