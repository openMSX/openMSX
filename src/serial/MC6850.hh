#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "MidiInConnector.hh"
#include "MidiOutConnector.hh"
#include "Schedulable.hh"
#include "openmsx.hh"
#include "outer.hh"

class MSXMotherBoard;
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
		SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncRecv);
			mc6850.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans : Schedulable {
		friend class MC6850;
		SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& mc6850 = OUTER(MC6850, syncTrans);
			mc6850.execTrans(time);
		}
	} syncTrans;
	void execRecv (EmuTime::param time);
	void execTrans(EmuTime::param time);

	void send(byte value, EmuTime::param time);

	IRQHelper rxIRQ;
	IRQHelper txIRQ;
	bool rxReady;
	byte controlReg;
	byte receiveDataReg;   //<! Byte received from MIDI in connector.
	byte transmitDataReg;  //<! Next to-be-sent byte.
	byte transmitShiftReg; //<! Byte currently being sent.
	byte statusReg;

	MidiOutConnector outConnector;
};
SERIALIZE_CLASS_VERSION(MC6850, 3);

} // namespace openmsx

#endif
