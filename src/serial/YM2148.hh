#ifndef YM2148_HH
#define YM2148_HH

#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "outer.hh"

class MSXMotherBoard;
class Scheduler;

namespace openmsx {

class YM2148 : public MidiInConnector
{
public:
	YM2148(const std::string& name, MSXMotherBoard& motherBoard);
	void reset();

	void writeCommand(byte value);
	void writeData(byte value, EmuTime::param time);
	byte readStatus(EmuTime::param time);
	byte readData(EmuTime::param time);
	byte peekStatus(EmuTime::param time) const;
	byte peekData(EmuTime::param time) const;

	bool pendingIRQ() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MidiInConnector
	bool ready() override;
	bool acceptsData() override;
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	// Schedulable
	struct SyncRecv : Schedulable {
		friend class YM2148;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& ym2148 = OUTER(YM2148, syncRecv);
			ym2148.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans : Schedulable {
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
	byte rxBuffer;  //<! Byte received from MIDI in connector.
	byte txBuffer1; //<! The byte currently being send.
	byte txBuffer2; //<! The next to-be-send byte.
	byte status;
	byte commandReg;

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(YM2148, 2);

} // namespace openmsx

#endif
