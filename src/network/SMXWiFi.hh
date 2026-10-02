#ifndef SMXWIFI_HH
#define SMXWIFI_HH

#include "BooleanSetting.hh"
#include "EmulatedEsp32.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "RS232Connector.hh"
#include "Rom.hh"

#include "circular_buffer.hh"

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>

namespace openmsx {

class SMXWiFi final : public MSXDevice, public RS232Connector
{
public:
	explicit SMXWiFi(DeviceConfig& config);
	~SMXWiFi() override;

	// MSXDevice
	void powerDown(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;

	// Connector
	void plug(Pluggable& device, EmuTime time) override;
	void unplug(EmuTime time) override;

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
	void writeCommand(uint8_t value, EmuTime time);
	void resetFIFO();
	void pushToFifo(std::span<const uint8_t> data);

	// Endpoint routing: the MSX UART talks either to the real ESP32 on
	// the serial port (an RS232Raw whose port is actually open) or to the
	// emulated ESP32.
	[[nodiscard]] bool isRealEndpoint() const;
	void updateEndpoint(EmuTime time);
	// Pushes the current UART configuration to the plugged device (the
	// format is fixed 8N1; only the baud rate is configurable).
	void applyParamsToReal(EmuTime time) const;

	cb_queue<uint8_t> fifo;
	mutable std::mutex fifoMutex;

	// Emulated ESP32, used while no real ESP32 is reachable. Created in
	// the constructor body, after all members are constructed, so its
	// emulation thread can always use the FIFO through the sink callback.
	std::unique_ptr<EmulatedEsp32> emulated;

	// ROM at 0x4000-0x7FFF (ESPUNAPI_IO.rom)
	Rom rom;

	// Cached CPU reference for emulated-time waits (VDPIODelay pattern)
	MSXCPU& cpu;

	// Quick receive: assert WAIT when the UART FIFO is empty (status
	// bit 3 reports it, so the driver can fall back to polled reads).
	BooleanSetting quickRcvSetting;

	// Small scalars, grouped at the end (members are ordered
	// largest-alignment-first to avoid padding).
	unsigned uartSpeed = 0; // UART_SPEED_859372
	bool underrun = false;
	// Whether the real serial endpoint is currently active (tracked to
	// detect endpoint switches in updateEndpoint()).
	bool realEndpointActive = false;
};

} // namespace openmsx

#endif