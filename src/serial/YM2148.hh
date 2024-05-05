#ifndef YM2148_HH
#define YM2148_HH

#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "outer.hh"

namespace openmsx {

class MSXMotherBoard;
class Scheduler;

class YM2148 final : public MidiInConnector
{
public:
	YM2148(const std::string& name, MSXMotherBoard& motherBoard);
	void reset();

	void writeCommand(byte value);
	void writeData(byte value, EmuTime::param time);
	[[nodiscard]] byte readStatus(EmuTime::param time) const;
	[[nodiscard]] byte readData(EmuTime::param time);
	[[nodiscard]] byte peekStatus(EmuTime::param time) const;
	[[nodiscard]] byte peekData(EmuTime::param time) const;

	[[nodiscard]] bool pendingIRQ() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MidiInConnector
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	// Schedulable
	struct SyncRecv final : Schedulable {
		friend class YM2148;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& ym2148 = OUTER(YM2148, syncRecv);
			ym2148.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class YM2148;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& ym2148 = OUTER(YM2148, syncTrans);
			ym2148.execTrans(time);
		}
	} syncTrans;
	void execRecv (EmuTime::param time);
	void execTrans(EmuTime::param time);

	void send(byte value, EmuTime::param time);

	IRQHelper rxIRQ;
	IRQHelper txIRQ;
	bool rxReady;
	byte rxBuffer;      //<! Byte received from MIDI in connector.
	byte txBuffer1 = 0; //<! The byte currently being send.
	byte txBuffer2 = 0; //<! The next to-be-send byte.
	byte status;
	byte commandReg;

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(YM2148, 2);

} // namespace openmsx

#endif
