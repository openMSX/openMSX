#ifndef MC6850_HH
#define MC6850_HH

#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"

#include "DynamicClock.hh"
#include "IRQHelper.hh"
#include "Schedulable.hh"

#include "outer.hh"

#include <cstdint>

namespace openmsx {

class Scheduler;

class MC6850 final : public MidiInConnector
{
public:
	MC6850(const std::string& name, MSXMotherBoard& motherBoard, unsigned clockFreq);
	void reset(EmuTime time);

	[[nodiscard]] uint8_t readStatusReg() const;
	[[nodiscard]] uint8_t peekStatusReg() const;
	[[nodiscard]] uint8_t readDataReg();
	[[nodiscard]] uint8_t peekDataReg() const;
	void writeControlReg(uint8_t value, EmuTime time);
	void writeDataReg   (uint8_t value, EmuTime time);

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
	void recvByte(uint8_t value, EmuTime time) override;

	// Schedulable
	struct SyncRecv final : Schedulable {
		friend class MC6850;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& mc6850 = OUTER(MC6850, syncRecv);
			mc6850.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class MC6850;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& mc6850 = OUTER(MC6850, syncTrans);
			mc6850.execTrans(time);
		}
	} syncTrans;
	void execRecv (EmuTime time);
	void execTrans(EmuTime time);

	// External clock, divided by 1, 16 or 64.
	// Transmitted bits are synced to this clock
	DynamicClock txClock{EmuTime::zero()};
	const unsigned clockFreq;

	IRQHelper rxIRQ;
	IRQHelper txIRQ;
	bool rxReady;
	bool txShiftRegValid; //<! True iff txShiftReg contains a valid value
	bool pendingOVRN;     //<! Overrun detected but not yet reported.
	uint8_t rxDataReg;       //<! Byte received from MIDI in connector.
	uint8_t txDataReg = 0;   //<! Next to-be-sent byte.
	uint8_t txShiftReg = 0;  //<! Byte currently being sent.
	uint8_t controlReg;
	uint8_t statusReg;
	uint8_t charLen;         //<! #start- + #data- + #parity- + #stop-bits

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(MC6850, 3);

} // namespace openmsx

#endif
