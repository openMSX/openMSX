#ifndef UNAPINET_HH
#define UNAPINET_HH

#include "MSXDevice.hh"
#include "Socket.hh"
#include "UnapiNetWire.hh"

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

// UnapiNet - MSX-UNAPI TCP/IP bridge device
//
// I/O ports 0x28 (command, write-only) and 0x29 (data). Same range as the
// DenYoNet - both are UNAPI Ethernet bridges and don't coexist.
// Bridge between the MSX and BSD sockets on the host, speaking protocol v2
// (see unapinet/protocol-v2.md): every reply starts with a status byte.

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
	// The command port is write-only in v2: every reply, its status byte
	// included, is read from the data port. Whether a reply is pending is
	// resultPos < resultBuf.size().

	// Parameter buffer (written to 0x29 before the command). Capped at one
	// byte past the largest legal block, so a truncated block always fails
	// the receiving command's size check (rule 3).
	std::vector<uint8_t> paramBuf;

	// Result buffer (read from 0x29 after the command)
	std::vector<uint8_t> resultBuf;
	size_t resultPos = 0;

	// --- TCP connections ---
	static constexpr int MAX_TCP = 4;
	// Internal handles are 0-based array indices; only the wire protocol is
	// 1-based. Conversion happens in tcpForHandle()/udpForHandle() and in the
	// OPEN replies, nowhere else.
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
	// everything they call) and the socket thread (socketLoop):
	//
	// * Only the socket thread ever calls sock_close(). requestClose() (any
	//   thread) invalidates the connection's socket immediately - so the MSX
	//   can reuse the handle at once - and hands the raw fd to socksToClose;
	//   the socket thread closes it at the top of its next pass. The fd therefore
	//   stays open until then, so its number cannot be recycled while another
	//   thread is still sitting on it inside select()/recv().
	//   closeTcp()/closeUdp() close directly and are only for the destructor,
	//   which has already joined the socket thread.
	// * 'mutex' guards the buffers AND the endpoint metadata, so that TCP_STATE
	//   reads a coherent snapshot and the accept path publishes the state and
	//   the address together. The socket thread re-checks that the socket is still
	//   the one it was watching before publishing anything, so it can never
	//   act on a connection the MSX has closed and reopened underneath it.
	// * Hold at most one connection mutex at a time, and never across
	//   select()/accept()/recv()/connect(). Non-blocking send/shutdown/close
	//   under the lock are fine.
	struct TcpConnection {
		std::atomic<SOCKET> sock{OPENMSX_INVALID_SOCKET};
		std::atomic<TcpState> tcpState{TcpState::Closed};
		// FinWait1 = the MSX called TCP_CLOSE. The FIN goes out once sendBuf has
		// drained (finSent), and the connection is dropped when the peer closes
		// too, or when closeDeadline passes and it never does.
		bool finSent = false; // guarded by 'mutex'
		std::chrono::steady_clock::time_point closeDeadline; // guarded by 'mutex'
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
		uint16_t localPort = 0;   // guarded by 'mutex'
		bool     resident = false; // emulation thread only; nothing sets it
		                           // today (v2: every UDP socket is transient)
		std::deque<UdpDatagram> recvQueue; // guarded by 'mutex'
		std::mutex mutex;
	};
	std::array<UdpConnection, MAX_UDP> udp;

	// --- ICMP echo ---
	// The capability is latched ONCE at device start (platform support
	// compiled in AND the channel opening successfully); DETECT advertises
	// it, the ICMP commands are gated on it, and reset() does not re-probe.
	bool icmpAvailable = false;
	void* icmpChannel = nullptr; // Windows HANDLE from IcmpCreateFile()

	struct IcmpReply {
		uint32_t srcIp = 0;
		uint8_t  ttl = 0;
		uint16_t identifier = 0;
		uint16_t sequence = 0;
		uint16_t dataLen = 0;
	};
	std::deque<IcmpReply> icmpReplies; // guarded by icmpMutex
	// reset() bumps the generation so an echo still in flight inside the
	// worker cannot repopulate the queue reset just cleared.
	uint32_t icmpGeneration = 0;       // guarded by icmpMutex
	std::mutex icmpMutex; // protects icmpReplies, icmpGeneration,
	                      // icmpRequest, icmpPending
	std::condition_variable icmpCv; // a request was queued, or shutdown
	std::thread icmpWorker;
	bool icmpPending = false; // guarded by icmpMutex
	// ICMP request for worker to handle (guarded by icmpMutex: the worker
	// copies it out under the lock, so a new ICMP_SEND cannot tear it)
	struct IcmpRequest {
		uint32_t dstIp = 0;
		uint8_t  ttl = 0;
		uint16_t identifier = 0;
		uint16_t sequence = 0;
		uint16_t dataLen = 0;
	} icmpRequest;

	// --- Async DNS ---
	enum class DnsStatus : uint8_t { Idle = 0, InProgress = 1, Complete = 2, Error = 3 };
	// One persistent worker thread (dnsWorkerLoop) serves DNS_QUERY: the
	// emulation thread queues the hostname in 'request' and never blocks on
	// the resolver. getaddrinfo() has no reliable cancellation, so a lookup
	// in flight is disowned rather than stopped: 'request' stays engaged
	// while its lookup runs and doubles as the ownership token - the worker
	// publishes its outcome (and clears 'request') only if 'request' still
	// holds the name it resolved. reset() clears 'request', so a lookup
	// that finishes after a reset cannot overwrite the state the reset
	// established. Invariant: request.has_value() iff status == InProgress.
	struct {
		DnsStatus status = DnsStatus::Idle; // guarded by 'mutex'
		uint32_t resolvedIp = 0;            // guarded by 'mutex'
		std::optional<std::string> request; // guarded by 'mutex'
		std::mutex mutex;
		std::condition_variable cv; // a request was queued, or shutdown
	} dns;
	std::thread dnsThread; // the persistent worker
	void dnsWorkerLoop();

	// --- Network socket thread ---
	// (One loop serves all sockets: it receives, flushes queued sends,
	// completes non-blocking connects and performs every close.)
	std::thread sockThread;
	std::atomic<bool> running{false};
	void socketLoop();

	// Loopback wake pair. The socket thread parks in select() for up to
	// 100 ms at a time; wakeRecv sits in every read set and poke() sends
	// it one byte, so new work from the emulation thread - a queued send,
	// a fresh socket to watch, a handed-over close, shutdown - is noticed
	// immediately instead of at the next timeout. (A UDP pair bound to
	// 127.0.0.1, because Windows select() only takes sockets - the POSIX
	// self-pipe trick spelled with what both platforms have.) The
	// constructor either creates the pair or throws MSXException, so both
	// sockets are always valid here: poke() is a guaranteed wake-up, and
	// the socket loop always has at least this fd in its select() set.
	SOCKET wakeRecv = OPENMSX_INVALID_SOCKET;
	SOCKET wakeSend = OPENMSX_INVALID_SOCKET;
	void poke();

	// --- Command processing ---
	void processCmd(uint8_t cmd);
	void cmdDetect();
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
	// The setResult() overloads queue a command reply for the MSX to read
	// from the data port (in v2 every reply, success or error, begins with
	// the status byte).
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
	// Single-byte reply: {ERR_OK} for the commands whose success carries no
	// data, {error code} for every failure (v2 rule 2).
	void replyStatus(uint8_t status);

	// Rule 3 helper: accept the parameter block only at exactly sizeof(T).
	// Commands with a variable payload (TCP_SEND / UDP_SEND) check their
	// combined size by hand instead.
	template<wire_layout T>
	[[nodiscard]] std::optional<T> exactParams() const
	{
		if (paramBuf.size() != sizeof(T)) return std::nullopt;
		return fromBytes<T>(paramBuf);
	}

	// Returns a free 0-based index, or INVALID_HANDLE.
	[[nodiscard]] int allocTcpHandle();
	// Validate a 1-based wire handle and return the connection, or nullptr.
	[[nodiscard]] TcpConnection* tcpForHandle(int wireHandle);
	// Direct close. Only legal with the socket thread stopped (destructor).
	void closeTcp(TcpConnection& c);
	// Drop a connection now: the socket is invalidated (so the handle is free
	// again immediately) and the raw fd is queued for the socket thread to close.
	// Safe from either thread. The buffers and endpoint info are LEFT in
	// place - a closed slot keeps its final state, close reason and undrained
	// receive data until TCP_OPEN reuses it (the v2 sticky-slot rule; ABORT
	// included). clearMetadata wipes them too, which only reset() wants.
	void requestClose(TcpConnection& c, CloseReason reason,
	                  bool clearMetadata = false);
	void requestClose(UdpConnection& u);
	// TCP_CLOSE: half-close (FIN) and let the peer finish, rather than dropping
	// data the MSX has already been told we accepted. Idempotent while the
	// close is in progress.
	void gracefulClose(TcpConnection& c);
	// Hand a raw fd to the socket thread to close.
	void deferSockClose(SOCKET sd);
	std::vector<SOCKET> socksToClose; // guarded by closeMutex
	std::mutex closeMutex; // lock order: a connection mutex may be held when
	                       // taking this one, never the other way round
	[[nodiscard]] int allocUdpHandle();
	[[nodiscard]] UdpConnection* udpForHandle(int wireHandle);
	void closeUdp(UdpConnection& u);
	void closeAllConnections();

};

} // namespace openmsx

#endif // UNAPINET_HH
