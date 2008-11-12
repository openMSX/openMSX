// $Id$

#ifndef WD2793_HH
#define WD2793_HH

#include "Disk.hh"
#include "Clock.hh"
#include "Schedulable.hh"

namespace openmsx {

class Scheduler;
class DiskDrive;

class WD2793 : public Schedulable
{
public:
	WD2793(Scheduler& scheduler, DiskDrive& drive, EmuTime::param time);

	void reset(EmuTime::param time);

	byte getStatusReg(EmuTime::param time);
	byte getTrackReg (EmuTime::param time);
	byte getSectorReg(EmuTime::param time);
	byte getDataReg  (EmuTime::param time);

	byte peekStatusReg(EmuTime::param time);
	byte peekTrackReg (EmuTime::param time);
	byte peekSectorReg(EmuTime::param time);
	byte peekDataReg  (EmuTime::param time);

	void setCommandReg(byte value, EmuTime::param time);
	void setTrackReg  (byte value, EmuTime::param time);
	void setSectorReg (byte value, EmuTime::param time);
	void setDataReg   (byte value, EmuTime::param time);

	bool getIRQ (EmuTime::param time);
	bool getDTRQ(EmuTime::param time);

	bool peekIRQ (EmuTime::param time);
	bool peekDTRQ(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum FSMState {
		FSM_NONE,
		FSM_SEEK,
		FSM_TYPE2_WAIT_LOAD,
		FSM_TYPE2_LOADED,
		FSM_TYPE2_ROTATED,
		FSM_TYPE3_WAIT_LOAD,
		FSM_TYPE3_LOADED,
		FSM_IDX_IRQ
	};

private:
	virtual void executeUntil(EmuTime::param time, int state);
	virtual const std::string& schedName() const;

	void startType1Cmd(EmuTime::param time);

	void seek(EmuTime::param time);
	void step(EmuTime::param time);
	void seekNext(EmuTime::param time);
	void endType1Cmd();

	void startType2Cmd(EmuTime::param time);
	void type2WaitLoad(EmuTime::param time);
	void type2Loaded(EmuTime::param time);
	void type2Rotated();

	void startType3Cmd(EmuTime::param time);
	void type3WaitLoad(EmuTime::param time);
	void type3Loaded(EmuTime::param time);
	void readAddressCmd();
	void readTrackCmd();
	void writeTrackCmd(EmuTime::param time);
	void endWriteTrackCmd();

	void startType4Cmd(EmuTime::param time);

	void endCmd();

	void tryToReadSector();
	inline void resetIRQ();
	inline void setIRQ();
	void setDRQ(bool drq, EmuTime::param time);

	void schedule(FSMState state, EmuTime::param time);

	DiskDrive& drive;

	EmuTime commandStart;
	Clock<1000000> DRQTimer; // us

	FSMState fsmState;
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

	byte dataBuffer[Disk::RAWTRACK_SIZE];
	int dataCurrent;   // which byte in dataBuffer is next to be read/write
	int dataAvailable; // how many bytes left in buffer
};

} // namespace openmsx

#endif
