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
#include <optional>
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
	// The status register alone describes the state: STATUS_DATA means a
	// result is waiting to be read from the data port, anything else means
	// idle. (STATUS_* live in the .cc.)
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

	// Threading contract between the emulation thread (readIO/writeIO and
	// everything they call) and the receiver thread (receiverLoop):
	//
	// * While the receiver thread runs, only IT closes a live socket. Any
	//   other thread calls requestClose(), which marks the connection closed
	//   for the MSX at once and sets 'pendingClose'; the receiver performs the
	//   sock_close() on its next pass (bounded by its select() timeout).
	//   Closing an fd another thread is sitting on inside select()/recv() is
	//   not safe: the descriptor can be recycled underneath it.
	//   closeTcp()/closeUdp() close directly and are only for the destructor,
	//   which has already joined the receiver.
	// * 'mutex' guards the buffers AND the endpoint metadata, so that TCP_STATE
	//   reads a coherent snapshot and the accept path publishes the state and
	//   the address together. The receiver re-checks 'pendingClose' under the
	//   lock before publishing anything, so it can never resurrect a
	//   connection the MSX has just closed.
	// * Hold at most one connection mutex at a time, and never across
	//   select()/accept()/recv()/connect(). Non-blocking send/shutdown/close
	//   under the lock are fine.
	struct TcpConnection {
		std::atomic<SOCKET> sock{OPENMSX_INVALID_SOCKET};
		std::atomic<TcpState> tcpState{TcpState::Closed};
		std::atomic<bool> pendingClose{false}; // receiver: close it and clear this
		CloseReason closeReason = CloseReason::NeverUsed; // guarded by 'mutex'
		uint32_t remoteIp = 0;    // guarded by 'mutex'
		uint16_t remotePort = 0;  // guarded by 'mutex'
		uint16_t localPort = 0;   // guarded by 'mutex'
		bool     resident = false; // emulation thread only
		std::deque<uint8_t> recvBuf;  // guarded by 'mutex'
		std::vector<uint8_t> sendBuf; // guarded by 'mutex'
		std::mutex mutex;
	};
	std::array<TcpConnection, MAX_TCP> tcp; // indexed by 0-based internal handle

	// --- UDP connections ---
	static constexpr int MAX_UDP = 4;

	struct UdpDatagram {
		uint32_t srcIp = 0;
		uint16_t srcPort = 0;
		std::vector<uint8_t> data;
	};

	struct UdpConnection { // same threading contract as TcpConnection above
		std::atomic<SOCKET> sock{OPENMSX_INVALID_SOCKET};
		std::atomic<bool> pendingClose{false};
		uint16_t localPort = 0;   // guarded by 'mutex'
		bool     resident = false; // emulation thread only
		std::deque<UdpDatagram> recvQueue; // guarded by 'mutex'
		std::mutex mutex;
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
	// Direct close. Only legal with the receiver thread stopped (destructor).
	void closeTcp(TcpConnection& c);
	// Mark a connection closed for the MSX right away and let the receiver
	// thread do the actual sock_close(). Safe from either thread.
	// clearMetadata also wipes the endpoint info and the buffers (what
	// TCP_CLOSE / TCP_ABORT / reset want); the receiver's error paths leave
	// them, so the MSX can still drain data that already arrived.
	void requestClose(TcpConnection& c, CloseReason reason,
	                  bool clearMetadata = false);
	void requestClose(UdpConnection& u);
	[[nodiscard]] int allocUdpHandle();
	[[nodiscard]] UdpConnection* udpForHandle(int wireHandle);
	void closeUdp(UdpConnection& u);
	void closeAllConnections();

};

} // namespace openmsx

#endif // UNAPINET_HH
