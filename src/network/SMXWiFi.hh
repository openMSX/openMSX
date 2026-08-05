#ifndef SMXWIFI_HH
#define SMXWIFI_HH

#include "MSXDevice.hh"
#include "RS232Connector.hh"

#include "circular_buffer.hh"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace openmsx {

class SMXWiFi final : public MSXDevice, public RS232Connector
{
public:
	explicit SMXWiFi(DeviceConfig& config);
	~SMXWiFi() override;

	// MSXDevice
	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	// Connector
	void plug(Pluggable& device, EmuTime time) override;

	// RS232Connector
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void recvByte(uint8_t value, EmuTime time) override;
	[[nodiscard]] bool ready() override;
	[[nodiscard]] bool acceptsData() override;
	[[nodiscard]] bool directByteDelivery() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint8_t readFIFO();
	[[nodiscard]] uint8_t readStatus();
	void writeCommand(uint8_t value);
	void resetFIFO();

	cb_queue<uint8_t> fifo;
	mutable std::mutex fifoMutex;
	std::condition_variable dataReady;

	bool underrun = false;
	unsigned uartSpeed = 0; // UART_SPEED_859372
};

} // namespace openmsx

#endif
