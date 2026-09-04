#ifndef EMULATEDESP32_HH
#define EMULATEDESP32_HH

#include "BooleanSetting.hh"
#include "DeviceConfig.hh"
#include "EmuTime.hh"
#include "Socket.hh"
#include "circular_buffer.hh"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace openmsx {

// Emulates the ESP32 of the SMXWiFi device: it presents the same variable
// serial contract as RS232Raw (recvByte + setBaudRate — the SM-X WiFi UART
// only ever changes the baud rate; the format is fixed 8N1) and implements
// the ESP32 UNAPI firmware: the command parser, the boot sequence and the
// full TCP/IP-UNAPI command set over the host network. Bytes produced by
// the emulation are delivered through a callback sink provided by the
// owner (which routes them like bytes from a real ESP32 on a serial port).
class EmulatedEsp32
{
public:
	EmulatedEsp32(const DeviceConfig& config, std::function<void(uint8_t)> sink);
	~EmulatedEsp32();

	// Serial contract (same variable part as RS232Raw): the owner routes
	// MSX-side bytes and baud-rate changes to this endpoint while it is
	// the active one.
	void recvByte(uint8_t value, EmuTime time);
	void setBaudRate(unsigned baud) const;

	// Closes all network connections and clears the parser, boot
	// sequence and pending input: called on MSX reset and whenever the
	// owner switches to or from this endpoint, so the emulation always
	// starts clean after a migration.
	void resetState();
	// Clears the pending MSX->ESP input (UART FIFO reset command).
	void resetFifo();

	// Thread lifecycle. The thread is started in the constructor (when
	// the extension is inserted into an already-running machine,
	// powerUp() is never called on the new device) and stopped on
	// destruction, power down and machine reset.
	void start();
	void stop();

private:
	// ESP emulator thread
	void espThreadFunc();
	void pushBootText(std::string_view text) const;
	void resetParser();
	// Dispatch a complete command. Must be called without msxToEspMutex
	// held.
	void processCommand();
	// Wait logic for an empty MSX->ESP FIFO. Returns true when the thread
	// should stop; on return 'unlocked' may hold work that must run
	// without the mutex held.
	[[nodiscard]] bool espWaitForWork(std::unique_lock<std::mutex>& lock,
	                                  std::function<void()>& unlocked);
	// Parser state machine for one received byte. On return 'unlocked'
	// may hold work that must run without the mutex held.
	void espParseByte(uint8_t b, std::function<void()>& unlocked);
	void tcpReaderThreadFunc(int connIdx);

	// Connection management
	class Connection;
	struct ListenSocket;
	static constexpr int MAX_CONNECTIONS = 4;

	// TCP reader-thread helpers (keep the reader loop shallow).
	void tcpAcceptClient(Connection* conn) const;
	void tcpDriveHandshake(Connection* conn, int connIdx);
	void tcpTlsFail(Connection* conn, int connIdx, uint8_t reason);
	// Reads inbound data; returns true when the reader loop must stop
	// (the remote side closed the connection or TLS failed).
	[[nodiscard]] bool tcpReadData(Connection* conn, int connIdx);
	void tcpDrainSsl(Connection* conn, int connIdx);
	void tcpReadClient(Connection* conn) const;
	// Polls the connection's sockets (never blocks) and handles whatever
	// is ready. Returns false when the reader loop must stop.
	[[nodiscard]] bool tcpPollConnection(Connection* conn, int connIdx);

	Connection* allocateConnection();
	void resetConnectionSlot(int i);
	void freeConnection(int idx);
	Connection* getConnection(int idx) const;
	int getFreeConnectionSlot() const;
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
	void cmdGetSettings(std::span<const uint8_t> data);
	void cmdAutoClock(uint8_t cmd, std::span<const uint8_t> data);
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
	void cmdTcpOpenPassive(uint16_t localPort, uint8_t flags);
	void cmdTcpOpenActive(uint32_t remoteIP, uint16_t remotePort,
	                      uint16_t localPort, uint8_t flags,
	                      const std::string& hostname,
	                      bool useTls, bool verifyCert);
	void cmdTcpClose(std::span<const uint8_t> data);
	void cmdTcpAbort(std::span<const uint8_t> data);
	void cmdTcpState(std::span<const uint8_t> data);
	void cmdTcpSend(std::span<const uint8_t> data);
	void cmdTcpRcv(std::span<const uint8_t> data);
	void cmdCfgAutoIP(std::span<const uint8_t> data);
	void cmdCfgIP(std::span<const uint8_t> data);

	// Convert IP address string to 4 bytes
	[[nodiscard]] bool parseIP(const std::string& str, std::span<uint8_t, 4> ipOut) const;
	// Random ephemeral port (spec 4.4.1/4.5.1), never one used by another
	// open connection.
	void pickEphemeralTcpPort(uint16_t& port);

	// Settings
	BooleanSetting enabledSetting;

	// MSX->ESP FIFO
	cb_queue<uint8_t> msxToEspFifo;
	mutable std::mutex msxToEspMutex;
	std::condition_variable msxToEspCond;

	// ESP thread
	std::thread espThread;
	std::atomic<bool> espRunning{false};

	// Set by cmdReset (running in the ESP thread); the ESP thread then
	// wipes pending input and emulates the device reboot sequence.
	bool bootStartPending = false;

	// ESP firmware emulation state, driven by espThreadFunc only:
	//  - the UART parser (CMD + 2-byte size + data framing)
	//  - the boot sequence ("Ready\r\n" retries after a reset)
	enum class ParserState : uint8_t { IDLE, WAIT_DATA_SIZE, GET_DATA };
	ParserState parserState = ParserState::IDLE;
	uint8_t parserCmdByte = 0;
	uint8_t parserSizeStep = 0;
	uint16_t parserExpectedSize = 0;
	std::vector<uint8_t> parserDataBuf;
	std::chrono::steady_clock::time_point parserDeadline;

	enum class BootStage : uint8_t { NONE, READY_LOOP };
	BootStage bootStage = BootStage::NONE;
	uint8_t bootReadyRetries = 0;
	std::chrono::steady_clock::time_point bootEventTime;

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

	// Byte delivery into the owner's UART FIFO (thread-safe by the
	// owner's FIFO mutex).
	std::function<void(uint8_t)> sink;
};

} // namespace openmsx

#endif