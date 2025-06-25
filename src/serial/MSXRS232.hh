#ifndef MSXRS232_HH
#define MSXRS232_HH

#include "I8251.hh"
#include "I8254.hh"
#include "IRQHelper.hh"
#include "MSXDevice.hh"
#include "RS232Connector.hh"

#include <memory>

namespace openmsx {

class Ram;
class Rom;
class BooleanSetting;

class MSXRS232 final : public MSXDevice, public RS232Connector
{
public:
	explicit MSXRS232(DeviceConfig& config);
	~MSXRS232() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	// TODO: implement peekMem, because the default isn't OK anymore
	[[nodiscard]] const uint8_t *getReadCacheLine(uint16_t start) const override;
	void writeMem(uint16_t address, uint8_t value, EmuTime time) override;
	[[nodiscard]] uint8_t* getWriteCacheLine(uint16_t start) override;
	[[nodiscard]] bool allowUnaligned() const override;

	// RS232Connector  (input)
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(uint8_t value, EmuTime time) override;
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint8_t readIOImpl(uint16_t port, EmuTime time);
	void writeIOImpl(uint16_t port, uint8_t value, EmuTime time);

	[[nodiscard]] uint8_t readStatus(EmuTime time);
	void setIRQMask(uint8_t value);
	void setRxRDYIRQ(bool status);
	void enableRxRDYIRQ(bool enabled);

	struct Counter0 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime time) override;
		void signalPosEdge(ClockPin& pin, EmuTime time) override;
	} cntr0; // counter 0 rx clock pin

	struct Counter1 final : ClockPinListener {
		void signal(ClockPin& pin, EmuTime time) override;
		void signalPosEdge(ClockPin& pin, EmuTime time) override;
	} cntr1; // counter 1 tx clock pin

	I8254 i8254;

	struct Interface final : I8251Interface {
		void setRxRDY(bool status, EmuTime time) override;
		void setDTR(bool status, EmuTime time) override;
		void setRTS(bool status, EmuTime time) override;
		[[nodiscard]] bool getDSR(EmuTime time) override;
		[[nodiscard]] bool getCTS(EmuTime time) override;
		void setDataBits(DataBits bits) override;
		void setStopBits(StopBits bits) override;
		void setParityBit(bool enable, Parity parity) override;
		void recvByte(uint8_t value, EmuTime time) override;
		void signal(EmuTime time) override;
	} interface;

	I8251 i8251;
	const std::unique_ptr<Rom> rom; // can be nullptr
	const std::unique_ptr<Ram> ram; // can be nullptr

	IRQHelper rxrdyIRQ;
	bool rxrdyIRQlatch = false;
	bool rxrdyIRQenabled = false;

	const bool hasMemoryBasedIo;
	const bool hasRIPin;
	const bool inputsPullup;
	bool ioAccessEnabled;

	const std::unique_ptr<BooleanSetting> switchSetting; // can be nullptr
};
SERIALIZE_CLASS_VERSION(MSXRS232, 2);


} // namespace openmsx

#endif
