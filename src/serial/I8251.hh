// This class implements the Intel 8251 chip (UART)

#ifndef I8251_HH
#define I8251_HH

#include "ClockPin.hh"
#include "SerialDataInterface.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
#include "openmsx.hh"
#include "outer.hh"

namespace openmsx {

class I8251Interface : public SerialDataInterface
{
public:
	virtual void setRxRDY(bool status, EmuTime::param time) = 0;
	virtual void setDTR(bool status, EmuTime::param time) = 0;
	virtual void setRTS(bool status, EmuTime::param time) = 0;

	[[nodiscard]] virtual bool getDSR(EmuTime::param time) = 0;
	[[nodiscard]] virtual bool getCTS(EmuTime::param time) = 0; // TODO use this
	virtual void signal(EmuTime::param time) = 0;

protected:
	I8251Interface() = default;
	~I8251Interface() = default;
};

class I8251 final : public SerialDataInterface
{
public:
	I8251(Scheduler& scheduler, I8251Interface& interface, EmuTime::param time);

	void reset(EmuTime::param time);
	[[nodiscard]] byte readIO(word port, EmuTime::param time);
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const;
	void writeIO(word port, byte value, EmuTime::param time);
	[[nodiscard]] ClockPin& getClockPin() { return clock; }
	[[nodiscard]] bool isRecvReady() const { return recvReady; }
	[[nodiscard]] bool isRecvEnabled() const;

	// SerialDataInterface
	void setDataBits(DataBits bits) override { recvDataBits = bits; }
	void setStopBits(StopBits bits) override { recvStopBits = bits; }
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	void execRecv(EmuTime::param time);
	void execTrans(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum class CmdPhase {
		MODE, SYNC1, SYNC2, CMD
	};

private:
	void setMode(byte newMode);
	void writeCommand(byte value, EmuTime::param time);
	[[nodiscard]] byte readStatus(EmuTime::param time);
	[[nodiscard]] byte readTrans(EmuTime::param time);
	void writeTrans(byte value, EmuTime::param time);
	void send(byte value, EmuTime::param time);

private:
	// Schedulable
	struct SyncRecv final : Schedulable {
		friend class I8251;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& i8251 = OUTER(I8251, syncRecv);
			i8251.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class I8251;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& i8251 = OUTER(I8251, syncTrans);
			i8251.execTrans(time);
		}
	} syncTrans;

	I8251Interface& interface;
	ClockPin clock;
	unsigned charLength;

	CmdPhase cmdPhase;
	SerialDataInterface::DataBits  recvDataBits;
	SerialDataInterface::StopBits  recvStopBits;
	SerialDataInterface::Parity recvParityBit;
	bool recvParityEnabled;
	byte recvBuf;
	bool recvReady;

	byte sendByte;
	byte sendBuffer;

	byte status;
	byte command;
	byte mode;
	byte sync1, sync2;
};
SERIALIZE_CLASS_VERSION(I8251, 2);

} // namespace openmsx

#endif
