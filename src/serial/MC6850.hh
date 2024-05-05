#ifndef MC6850_HH
#define MC6850_HH

#include "DynamicClock.hh"
#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "outer.hh"

namespace openmsx {

class Scheduler;

class MC6850 final : public MidiInConnector
{
public:
	MC6850(const std::string& name, MSXMotherBoard& motherBoard, unsigned clockFreq);
	void reset(EmuTime::param time);

	[[nodiscard]] byte readStatusReg() const;
	[[nodiscard]] byte peekStatusReg() const;
	[[nodiscard]] byte readDataReg();
	[[nodiscard]] byte peekDataReg() const;
	void writeControlReg(byte value, EmuTime::param time);
	void writeDataReg   (byte value, EmuTime::param time);

        template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setDataFormat();

	// MidiInConnector
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	// Schedulable
	struct SyncRecv final : Schedulable {
		friend class MC6850;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncRecv);
			mc6850.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class MC6850;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncTrans);
			mc6850.execTrans(time);
		}
	} syncTrans;
	void execRecv (EmuTime::param time);
	void execTrans(EmuTime::param time);

	// External clock, divided by 1, 16 or 64.
	// Transmitted bits are synced to this clock
	DynamicClock txClock{EmuTime::zero()};
	const unsigned clockFreq;

	IRQHelper rxIRQ;
	IRQHelper txIRQ;
	bool rxReady;
	bool txShiftRegValid; //<! True iff txShiftReg contains a valid value
	bool pendingOVRN;     //<! Overrun detected but not yet reported.
	byte rxDataReg;       //<! Byte received from MIDI in connector.
	byte txDataReg = 0;   //<! Next to-be-sent byte.
	byte txShiftReg = 0;  //<! Byte currently being sent.
	byte controlReg;
	byte statusReg;
	byte charLen;         //<! #start- + #data- + #parity- + #stop-bits

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(MC6850, 3);

} // namespace openmsx

#endif
