#ifndef UNAPINET_HH
#define UNAPINET_HH

#include "MSXDevice.hh"
#include "Socket.hh"
#include "UnapiNetWire.hh"

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

// UnapiNet - openMSX Extension (Phase 2)
//
// I/O ports 0x28 (cmd/status) and 0x29 (data). Same range as the
// DenYoNet - both are UNAPI Ethernet bridges and don't coexist.
// Bridge between the MSX and BSD sockets on the host.

namespace openmsx {

class UnapiNet final : public MSXDevice
{
public:
	explicit UnapiNet(const DeviceConfig& config);
	~UnapiNet() override;

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Keep the host socket subsystem initialized for this device's lifetime
	// (WSAStartup/WSACleanup on Windows). Empty type -> [[no_unique_address]].
	[[no_unique_address]] SocketActivator socketActivator;

	// --- I/O protocol state ---
	enum class State { IDLE, RESULT_READY };
	State    state = State::IDLE;
	uint8_t  statusReg = 0x00; // STATUS_OK

	// Parameter buffer (written to 0x29 before the command)
	std::vector<uint8_t> paramBuf;

	// Result buffer (read from 0x29 after the command)
	std::vector<uint8_t> resultBuf;
	size_t resultPos = 0;

	// --- TCP connections ---
	static constexpr int MAX_TCP = 4;
	// Internal handles are 0-based array indices; only the wire protocol is
	// 1-based (a 0 handle byte means failure there). Conversion happens in
	// tcpForHandle()/udpForHandle() and in the OPEN replies, nowhere else.
	static constexpr int INVALID_HANDLE = -1;

	// TCP states (UNAPI spec wire values)
	enum class TcpState : uint8_t {
		Closed      = 0,
		Listen      = 1,
		SynSent     = 2,
		SynRecv     = 3,
		Established = 4,
		FinWait1    = 5,
		FinWait2    = 6,
		CloseWait   = 7,
		Closing     = 8,
		LastAck     = 9,
		TimeWait    = 10,
	};

	// Wire-defined TCP close-reason codes (read back by the UNAPI TSR).
	enum class CloseReason : uint8_t {
		None = 0, NeverUsed = 1, ClosedByUser = 2,
		Aborted = 3, ConnectionReset = 4, ConnectFailed = 6,
	};

	struct TcpConnection {
		SOCKET sock = OPENMSX_INVALID_SOCKET;
		std::atomic<TcpState> tcpState{TcpState::Closed};
		CloseReason closeReason = CloseReason::NeverUsed;
		bool     resident = false;
		uint32_t remoteIp = 0;
		uint16_t remotePort = 0;
		uint16_t localPort = 0;
		bool     connecting = false;
		std::deque<uint8_t> recvBuf; // guarded by 'mutex'
		std::mutex mutex; // protects recvBuf only. tcpState is atomic; the
		                  // remaining metadata fields are plain and shared
		                  // between the emulation and receiver threads (see
		                  // 'known limitations' in the PR description)
	};
	std::array<TcpConnection, MAX_TCP> tcp; // indexed by 0-based internal handle

	// --- UDP connections ---
	static constexpr int MAX_UDP = 4;

	struct UdpDatagram {
		uint32_t srcIp = 0;
		uint16_t srcPort = 0;
		std::vector<uint8_t> data;
	};

	struct UdpConnection {
		SOCKET sock = OPENMSX_INVALID_SOCKET;
		uint16_t localPort = 0;
		bool     resident = false;
		std::deque<UdpDatagram> recvQueue; // guarded by 'mutex'
		std::mutex mutex; // protects recvQueue only
	};
	std::array<UdpConnection, MAX_UDP> udp;

	// --- ICMP echo reply queue ---
	struct IcmpReply {
		uint32_t srcIp = 0;
		uint8_t  ttl = 0;
		uint16_t identifier = 0;
		uint16_t sequence = 0;
		uint16_t dataLen = 0;
	};
	std::deque<IcmpReply> icmpReplies; // guarded by icmpMutex
	std::mutex icmpMutex; // protects icmpReplies only
	std::thread icmpWorker;
	std::atomic<bool> icmpPending{false};
	// ICMP request for worker to handle
	struct IcmpRequest {
		uint32_t dstIp = 0;
		uint8_t  ttl = 0;
		uint16_t identifier = 0;
		uint16_t sequence = 0;
		uint16_t dataLen = 0;
	} icmpRequest;

	// --- Async DNS ---
	enum class DnsStatus : uint8_t { Idle = 0, InProgress = 1, Complete = 2, Error = 3 };
	struct {
		std::atomic<DnsStatus> status{DnsStatus::Idle};
		uint32_t resolvedIp = 0;
		uint8_t  errorCode = 0;
	} dns;
	std::thread dnsThread;

	// --- Network receiver thread ---
	std::thread recvThread;
	std::atomic<bool> running{false};
	void receiverLoop();

	// --- Command processing ---
	void processCmd(uint8_t cmd);
	void cmdPing();
	void cmdQueryCap();
	void cmdDnsQuery();
	void cmdDnsStatus();
	void cmdTcpOpen();
	void cmdTcpSend();
	void cmdTcpRecv();
	void cmdTcpClose();
	void cmdTcpState();
	void cmdTcpAbort();
	void cmdGetLocalIP();
	void cmdNetState();
	void cmdUdpOpen();
	void cmdUdpClose();
	void cmdUdpState();
	void cmdUdpSend();
	void cmdUdpRecv();
	void cmdIcmpSend();
	void cmdIcmpRecv();
	void icmpWorkerLoop();

	// --- Helpers ---
	// The three setResult() overloads queue a command result for the MSX to
	// read from the data port (and set state/statusReg accordingly).
	void setResult(std::span<const uint8_t> data);
	// Wire-layout struct (see UnapiNetWire.hh): the compiler lays out the
	// exact on-wire bytes. The requires-clause keeps span-like types (which
	// accidentally satisfy wire_layout, being trivially-copyable objects
	// holding a pointer) on the span overload above - serializing a span
	// OBJECT would emit its pointer bytes, not the pointed-to data.
	template<wire_layout T>
		requires (!std::convertible_to<const T&, std::span<const uint8_t>>)
	void setResult(const T& d)
	{
		setResult(asBytes(d));
	}
	// Fixed-size header struct followed by a variable payload
	// (used by TCP_RECV / UDP_RECV).
	template<wire_layout T>
		requires (!std::convertible_to<const T&, std::span<const uint8_t>>)
	void setResult(const T& hdr, std::span<const uint8_t> payload)
	{
		setResult(asBytes(hdr));
		resultBuf.insert(resultBuf.end(), payload.begin(), payload.end());
	}
	void setResultByte(uint8_t b);
	void setError();

	// Returns a free 0-based index, or INVALID_HANDLE.
	[[nodiscard]] int allocTcpHandle();
	// Validate a 1-based wire handle and return the connection, or nullptr.
	[[nodiscard]] TcpConnection* tcpForHandle(int wireHandle);
	void closeTcp(TcpConnection& c);
	// Quick teardown of a live connection from the receiver/send paths: record
	// the reason, close the socket and mark it CLOSED (does not clear the
	// remote/local metadata — that is closeTcp's job).
	void forceClose(TcpConnection& c, CloseReason reason);
	[[nodiscard]] int allocUdpHandle();
	[[nodiscard]] UdpConnection* udpForHandle(int wireHandle);
	void closeUdp(UdpConnection& u);
	void closeAllConnections();

};

} // namespace openmsx

#endif // UNAPINET_HH
