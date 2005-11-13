// $Id$

#ifndef WD2793_HH
#define WD2793_HH

#include "Clock.hh"
#include "Schedulable.hh"

namespace openmsx {

class Scheduler;
class DiskDrive;

class WD2793 : private Schedulable
{
public:
	WD2793(Scheduler& scheduler, DiskDrive& drive, const EmuTime& time);

	void reset(const EmuTime& time);

	byte getStatusReg(const EmuTime& time);
	byte getTrackReg (const EmuTime& time);
	byte getSectorReg(const EmuTime& time);
	byte getDataReg  (const EmuTime& time);

	byte peekStatusReg(const EmuTime& time);
	byte peekTrackReg (const EmuTime& time);
	byte peekSectorReg(const EmuTime& time);
	byte peekDataReg  (const EmuTime& time);

	void setCommandReg(byte value, const EmuTime& time);
	void setTrackReg  (byte value, const EmuTime& time);
	void setSectorReg (byte value, const EmuTime& time);
	void setDataReg   (byte value, const EmuTime& time);

	bool getIRQ (const EmuTime& time);
	bool getDTRQ(const EmuTime& time);

	bool peekIRQ (const EmuTime& time);
	bool peekDTRQ(const EmuTime& time);

private:
	enum FSMState {
		FSM_NONE,
		FSM_SEEK,
		FSM_TYPE2_WAIT_LOAD,
		FSM_TYPE2_LOADED,
		FSM_TYPE2_ROTATED,
		FSM_TYPE3_WAIT_LOAD,
		FSM_TYPE3_LOADED,
		FSM_IDX_IRQ
	} fsmState;

	virtual void executeUntil(const EmuTime& time, int state);
	virtual const std::string& schedName() const;

	void startType1Cmd(const EmuTime& time);

	void seek(const EmuTime& time);
	void step(const EmuTime& time);
	void seekNext(const EmuTime& time);
	void endType1Cmd();

	void startType2Cmd(const EmuTime& time);
	void type2WaitLoad(const EmuTime& time);
	void type2Loaded(const EmuTime& time);
	void type2Rotated();

	void startType3Cmd(const EmuTime& time);
	void type3WaitLoad(const EmuTime& time);
	void type3Loaded(const EmuTime& time);
	void readAddressCmd();
	void readTrackCmd();
	void writeTrackCmd(const EmuTime& time);

	void startType4Cmd(const EmuTime& time);

	void endCmd();

	void tryToReadSector();
	inline void resetIRQ();
	inline void setIRQ();
	void setDRQ(bool drq, const EmuTime& time);

	void schedule(FSMState state, const EmuTime& time);

	DiskDrive& drive;

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
	bool formatting;
	bool needInitWriteTrack;

	byte dataBuffer[1024];	// max sector size possible
	int dataCurrent;	// which byte in dataBuffer is next to be read/write
	int dataAvailable;	// how many bytes left in sector
};

} // namespace openmsx

#endif
