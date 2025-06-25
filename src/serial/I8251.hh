// This class implements the Intel 8251 chip (UART)

#ifndef I8251_HH
#define I8251_HH

#include "ClockPin.hh"
#include "SerialDataInterface.hh"

#include "Schedulable.hh"
#include "serialize_meta.hh"

#include "outer.hh"

#include <cstdint>

namespace openmsx {

class I8251Interface : public SerialDataInterface
{
public:
	virtual void setRxRDY(bool status, EmuTime time) = 0;
	virtual void setDTR(bool status, EmuTime time) = 0;
	virtual void setRTS(bool status, EmuTime time) = 0;

	[[nodiscard]] virtual bool getDSR(EmuTime time) = 0;
	[[nodiscard]] virtual bool getCTS(EmuTime time) = 0; // TODO use this
	virtual void signal(EmuTime time) = 0;

protected:
	I8251Interface() = default;
	~I8251Interface() = default;
};

class I8251 final : public SerialDataInterface
{
public:
	I8251(Scheduler& scheduler, I8251Interface& interface, EmuTime time);

	void reset(EmuTime time);
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time);
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const;
	void writeIO(uint16_t port, uint8_t value, EmuTime time);
	[[nodiscard]] ClockPin& getClockPin() { return clock; }
	[[nodiscard]] bool isRecvReady() const { return recvReady; }
	[[nodiscard]] bool isRecvEnabled() const;

	// SerialDataInterface
	void setDataBits(DataBits bits) override { recvDataBits = bits; }
	void setStopBits(StopBits bits) override { recvStopBits = bits; }
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(uint8_t value, EmuTime time) override;

	void execRecv(EmuTime time);
	void execTrans(EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialize
	enum class CmdPhase : uint8_t {
		MODE, SYNC1, SYNC2, CMD
	};

private:
	void setMode(uint8_t newMode);
	void writeCommand(uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t readStatus(EmuTime time);
	[[nodiscard]] uint8_t readTrans(EmuTime time);
	void writeTrans(uint8_t value, EmuTime time);
	void send(uint8_t value, EmuTime time);

private:
	// Schedulable
	struct SyncRecv final : Schedulable {
		friend class I8251;
		explicit SyncRecv(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& i8251 = OUTER(I8251, syncRecv);
			i8251.execRecv(time);
		}
	} syncRecv;
	struct SyncTrans final : Schedulable {
		friend class I8251;
		explicit SyncTrans(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
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
	uint8_t recvBuf;
	bool recvReady;

	uint8_t sendByte;
	uint8_t sendBuffer;

	uint8_t status;
	uint8_t command;
	uint8_t mode;
	uint8_t sync1, sync2;
};
SERIALIZE_CLASS_VERSION(I8251, 2);

} // namespace openmsx

#endif
