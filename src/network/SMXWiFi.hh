#ifndef SMXWIFI_HH
#define SMXWIFI_HH

#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "RS232Connector.hh"
#include "Rom.hh"

#include "circular_buffer.hh"

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
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;

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
	[[nodiscard]] uint8_t readFIFO(EmuTime time);
	[[nodiscard]] uint8_t peekFIFO() const;
	[[nodiscard]] uint8_t readStatus();
	[[nodiscard]] uint8_t peekStatus() const;
	[[nodiscard]] uint8_t peekStatusLocked() const;
	void writeCommand(uint8_t value);
	void resetFIFO();

	cb_queue<uint8_t> fifo;
	mutable std::mutex fifoMutex;

	bool underrun = false;
	unsigned uartSpeed = 0; // UART_SPEED_859372

	// ROM at 0x4000-0x7FFF (ESPUNAPI_IO.rom)
	Rom rom;

	// Cached CPU reference for emulated-time waits (VDPIODelay pattern)
	MSXCPU& cpu;
};

} // namespace openmsx

#endif
