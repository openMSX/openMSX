#ifndef GENERICUNAPI_HH
#define GENERICUNAPI_HH

#include "MSXDevice.hh"
#include "BooleanSetting.hh"
#include "Rom.hh"
#include "Socket.hh"
#include "circular_buffer.hh"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <thread>

namespace openmsx {

class GenericUNAPI final : public MSXDevice
{
public:
	explicit GenericUNAPI(DeviceConfig& config);
	~GenericUNAPI() override;

	// MSXDevice
	void powerDown(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// FIFO management
	[[nodiscard]] uint8_t readEspToMsxFifo();
	[[nodiscard]] uint8_t peekEspToMsxFifo() const;
	[[nodiscard]] uint8_t readStatus();
	[[nodiscard]] uint8_t peekStatus() const;
	[[nodiscard]] uint8_t peekStatusLocked() const;
	void writeCommand(uint8_t value);
	void resetFifo();

	// ESP emulator thread
	void espThreadFunc();
	void startEspThread();
	void stopEspThread();
	void tcpReaderThreadFunc(int connIdx);

	// Connection management
	struct Connection;
	struct ListenSocket;
	static constexpr int MAX_CONNECTIONS = 4;

	Connection* allocateConnection();
	void freeConnection(int idx);
	Connection* getConnection(int idx);
	int getFreeConnectionSlot();
	void releaseListenEntry(int idx);

	// Command dispatch
	void handleCustomCommand(uint8_t cmd, std::span<const uint8_t> data);
	void handleUnapiCommand(uint8_t cmd, std::span<const uint8_t> data);

	// Response helpers
	void sendQuickResponse(uint8_t cmdByte, uint8_t errorCode);
	void sendResponse(uint8_t cmdByte, uint8_t errorCode,
	                  std::span<const uint8_t> data = {});
	void sendRawResponse(std::span<const uint8_t> data);

	// Last response for Retry
	std::vector<uint8_t> lastResponse;
	void saveLastResponse(std::span<const uint8_t> data);

	// Custom command handlers
	void cmdReset(std::span<const uint8_t> data);
	void cmdWarmReset(std::span<const uint8_t> data);
	void cmdQuery(std::span<const uint8_t> data);
	void cmdGetVersion(std::span<const uint8_t> data);
	void cmdRetry(std::span<const uint8_t> data);
	void cmdScanAP(std::span<const uint8_t> data);
	void cmdScanResults(std::span<const uint8_t> data);
	void cmdConnectAP(std::span<const uint8_t> data);
	void cmdGetAPStatus(std::span<const uint8_t> data);
	void cmdFirmwareUpdate(uint8_t cmd, std::span<const uint8_t> data);
	void cmdSetBaud(std::span<const uint8_t> data);
	void cmdGetSettings(std::span<const uint8_t> data);
	void cmdGetAutoClock(std::span<const uint8_t> data);
	void cmdSetAutoClock(std::span<const uint8_t> data);
	void cmdSetWiFiTimer(std::span<const uint8_t> data);
	void cmdGetDateTime(std::span<const uint8_t> data);

	// UNAPI command handlers
	void unimplementedCmd(uint8_t cmd);
	void cmdGetInfo(std::span<const uint8_t> data);
	void cmdGetCapab(std::span<const uint8_t> data);
	void cmdGetIPInfo(std::span<const uint8_t> data);
	void cmdNetState(std::span<const uint8_t> data);
	void cmdDnsQ(std::span<const uint8_t> data);
	void cmdDnsQNew(std::span<const uint8_t> data);
	void cmdUdpOpen(std::span<const uint8_t> data);
	void cmdUdpClose(std::span<const uint8_t> data);
	void cmdUdpState(std::span<const uint8_t> data);
	void cmdUdpSend(std::span<const uint8_t> data);
	void cmdUdpRcv(std::span<const uint8_t> data);
	void cmdTcpOpen(std::span<const uint8_t> data);
	void cmdTcpClose(std::span<const uint8_t> data);
	void cmdTcpAbort(std::span<const uint8_t> data);
	void cmdTcpState(std::span<const uint8_t> data);
	void cmdTcpSend(std::span<const uint8_t> data);
	void cmdTcpRcv(std::span<const uint8_t> data);
	void cmdCfgAutoIP(std::span<const uint8_t> data);
	void cmdCfgIP(std::span<const uint8_t> data);

	// Convert IP address string to 4 bytes
	[[nodiscard]] bool parseIP(const std::string& str, std::span<uint8_t, 4> ipOut) const;

	// Settings
	BooleanSetting enabledSetting;

	// MSX->ESP FIFO
	cb_queue<uint8_t> msxToEspFifo;
	mutable std::mutex msxToEspMutex;
	std::condition_variable msxToEspCond;

	// ESP->MSX FIFO
	cb_queue<uint8_t> espToMsxFifo;
	mutable std::mutex espToMsxMutex;

	bool underrun = false;

	// ESP thread
	std::thread espThread;
	std::atomic<bool> espRunning{false};

	// Set by cmdReset (running in the ESP thread); the ESP thread then
	// wipes pending input and emulates the device reboot sequence.
	bool bootStartPending = false;

	// Connections
	mutable std::mutex connectionsMutex;
	std::array<std::unique_ptr<Connection>, MAX_CONNECTIONS> connections;

	// Close reason of the last connection that was freed (TCP-IP UNAPI
	// spec 4.5.4): kept when the connection is closed so that TCP_STATE
	// can still report the reason alongside ERR_NO_CONN. Written by the
	// reader thread (without holding connectionsMutex, to avoid a
	// deadlock with freeConnection joining the reader thread under that
	// mutex); cleared when the slot is reused.
	std::array<std::atomic<uint8_t>, MAX_CONNECTIONS> closeReason = {};

	// Passive TCP listeners: several passive connections may share one
	// listening socket for the same local port (TCP-IP UNAPI spec 1.1,
	// section 4.5.1). Guarded by connectionsMutex.
	std::vector<std::unique_ptr<ListenSocket>> listenSockets;

	// Socket subsystem init
	[[no_unique_address]] SocketActivator socketActivator;

	// ROM at 0x4000-0x7FFF (ESPUNAPI_IO.rom)
	Rom rom;
};

} // namespace openmsx

#endif
