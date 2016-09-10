#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"
#include "DynamicClock.hh"
#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "outer.hh"
#include "serialize_meta.hh"

class Scheduler;

namespace openmsx {

class MC6850 final : public MSXDevice, public MidiInConnector
{
public:
	explicit MC6850(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readStatusReg();
	byte peekStatusReg() const;
	byte readDataReg();
	byte peekDataReg() const;
	void writeControlReg(byte value, EmuTime::param time);
	void writeDataReg   (byte value, EmuTime::param time);
	void setDataFormat();

	// MidiInConnector
	bool ready() override;
	bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	// Schedulable
	struct SyncRecv : Schedulable {
		friend class MC6850;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncRecv);
			mc6850.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans : Schedulable {
		friend class MC6850;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncTrans);
			mc6850.execTrans(time);
		}
	} syncTrans;
	void execRecv (EmuTime::param time);
	void execTrans(EmuTime::param time);

	// External clock of 500kHz, divided by 1, 16 or 64.
	// Transmitted bits are synced to this clock
	DynamicClock txClock;

	IRQHelper rxIRQ;
	IRQHelper txIRQ;
	bool rxReady;
	bool txShiftRegValid; //<! True iff txShiftReg contains a valid value
	bool pendingOVRN;     //<! Overrun detected but not yet reported.
	byte rxDataReg;       //<! Byte received from MIDI in connector.
	byte txDataReg;       //<! Next to-be-sent byte.
	byte txShiftReg;      //<! Byte currently being sent.
	byte controlReg;
	byte statusReg;
	byte charLen;         //<! #start- + #data- + #parity- + #stop-bits

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(MC6850, 3);

} // namespace openmsx

#endif
