// $Id$

#ifndef __WD2793_HH__
#define __WD2793_HH__

#include "EmuTime.hh"
#include "Schedulable.hh"

namespace openmsx {

class DiskDrive;


class WD2793 : private Schedulable
{
public: 
	WD2793(DiskDrive* drive, const EmuTime& time);
	virtual ~WD2793();

	void reset(const EmuTime& time);
	
	byte getStatusReg(const EmuTime& time);
	byte getTrackReg (const EmuTime& time);
	byte getSectorReg(const EmuTime& time);
	byte getDataReg  (const EmuTime& time);
	
	void setCommandReg(byte value, const EmuTime& time);
	void setTrackReg  (byte value, const EmuTime& time);
	void setSectorReg (byte value, const EmuTime& time);
	void setDataReg   (byte value, const EmuTime& time);
	
	bool getIRQ (const EmuTime& time);
	bool getDTRQ(const EmuTime& time);

private:
	// Status register
	static const int BUSY             = 0x01;
	static const int INDEX            = 0x02;
	static const int S_DRQ            = 0x02;
	static const int TRACK00          = 0x04;
	static const int LOST_DATA        = 0x04;
	static const int CRC_ERROR        = 0x08;
	static const int SEEK_ERROR       = 0x10;
	static const int RECORD_NOT_FOUND = 0x10;
	static const int HEAD_LOADED      = 0x20;
	static const int RECORD_TYPE      = 0x20;
	static const int WRITE_PROTECTED  = 0x40;
	static const int NOT_READY        = 0x80;
	
	// Command register
	static const int STEP_SPEED = 0x03;
	static const int V_FLAG     = 0x04;
	static const int E_FLAG     = 0x04;
	static const int H_FLAG     = 0x08;
	static const int IMM_IRQ    = 0x08;
	static const int T_FLAG     = 0x10;
	static const int M_FLAG     = 0x10;
	
	enum FSMState {
		FSM_NONE,
		FSM_SEEK,
		FSM_TYPE2_WAIT_LOAD,
		FSM_TYPE2_LOADED,
		FSM_TYPE3_WAIT_LOAD,
		FSM_TYPE3_LOADED,
	} fsmState;
	virtual void executeUntil(const EmuTime& time, int state);
	virtual const string& schedName() const;

	void startType1Cmd(const EmuTime& time);

	void seek(const EmuTime& time);
	void step(const EmuTime& time);
	void seekNext(const EmuTime& time);
	void endType1Cmd();
	
	void startType2Cmd(const EmuTime& time);
	void type2WaitLoad(const EmuTime& time);
	void type2Loaded();
	
	void startType3Cmd(const EmuTime& time);
	void type3WaitLoad(const EmuTime& time);
	void type3Loaded(const EmuTime& time);
	void readAddressCmd();
	void readTrackCmd();
	void writeTrackCmd(const EmuTime& time);
	
	void startType4Cmd();
	
	void endCmd();

	void tryToReadSector();
	inline void resetIRQ();
	inline void setIRQ();

	void schedule(FSMState state, const EmuTime& time);

	DiskDrive* drive;
	
	EmuTime commandStart;
	Clock<1000000> DRQTimer;	// us

	byte statusReg;
	byte commandReg;
	byte sectorReg;
	byte trackReg;
	byte dataReg;

	bool directionIn;
	bool INTRQ;
	bool immediateIRQ;
	bool DRQ;
	bool transferring;

	byte onDiskTrack;
	byte onDiskSector;
	byte onDiskSide;
	int  onDiskSize;

	byte dataBuffer[1024];	// max sector size possible
	int dataCurrent;	// which byte in dataBuffer is next to be read/write
	int dataAvailable;	// how many bytes left in sector
};

} // namespace openmsx

#endif
