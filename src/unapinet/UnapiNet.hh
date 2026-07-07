#ifndef UNAPINET_HH
#define UNAPINET_HH

#include "MSXDevice.hh"
#include "Socket.hh"
#ifdef interface
#undef interface  // winsock2.h (pulled in by Socket.hh) #defines `interface`;
#endif            // undo it so it can't clobber other openMSX headers.
#include "UnapiNetWire.hh"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
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
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Keep the host socket subsystem initialized for this device's lifetime
	// (WSAStartup/WSACleanup on Windows). Empty type -> [[no_unique_address]].
	[[no_unique_address]] SocketActivator socketActivator;

	// --- I/O protocol state ---
	enum class State { IDLE, RESULT_READY };
	State    state;
	uint8_t  statusReg;

	// Parameter buffer (written to 0x29 before the command)
	std::vector<uint8_t> paramBuf;

	// Result buffer (read from 0x29 after the command)
	std::vector<uint8_t> resultBuf;
	size_t resultPos;

	// --- Bridge commands ---
	static constexpr uint8_t CMD_PING       = 0x00;
	static constexpr uint8_t CMD_DNS_QUERY  = 0x01;
	static constexpr uint8_t CMD_DNS_STATUS = 0x02;
	static constexpr uint8_t CMD_TCP_OPEN   = 0x03;
	static constexpr uint8_t CMD_TCP_SEND   = 0x04;
	static constexpr uint8_t CMD_TCP_RECV   = 0x05;
	static constexpr uint8_t CMD_TCP_CLOSE  = 0x06;
	static constexpr uint8_t CMD_TCP_STATE  = 0x07;
	static constexpr uint8_t CMD_TCP_ABORT  = 0x08;
	static constexpr uint8_t CMD_UDP_OPEN   = 0x09;
	static constexpr uint8_t CMD_UDP_CLOSE  = 0x0A;
	static constexpr uint8_t CMD_UDP_STATE  = 0x0B;
	static constexpr uint8_t CMD_UDP_SEND   = 0x0C;
	static constexpr uint8_t CMD_GET_LOCALIP = 0x0D;
	static constexpr uint8_t CMD_NET_STATE  = 0x0E;
	static constexpr uint8_t CMD_UDP_RECV   = 0x0F;
	static constexpr uint8_t CMD_QUERY_CAP  = 0x10;
	static constexpr uint8_t CMD_ICMP_SEND  = 0x11;
	static constexpr uint8_t CMD_ICMP_RECV  = 0x12;

	// --- Status register values ---
	static constexpr uint8_t STATUS_OK    = 0x00;
	static constexpr uint8_t STATUS_ERROR = 0x01;
	static constexpr uint8_t STATUS_DATA  = 0x02;

	static constexpr uint8_t MAGIC = 0xAB;

	// --- TCP connections ---
	static constexpr int MAX_TCP = 4;

	// TCP states (UNAPI spec)
	enum TcpState : uint8_t {
		TCP_CLOSED      = 0,
		TCP_LISTEN      = 1,
		TCP_SYN_SENT    = 2,
		TCP_SYN_RECV    = 3,
		TCP_ESTABLISHED = 4,
		TCP_FIN_WAIT1   = 5,
		TCP_FIN_WAIT2   = 6,
		TCP_CLOSE_WAIT  = 7,
		TCP_CLOSING     = 8,
		TCP_LAST_ACK    = 9,
		TCP_TIME_WAIT   = 10,
	};

	// Wire-defined TCP close-reason codes (read back by the UNAPI TSR).
	enum class CloseReason : uint8_t {
		None = 0, NeverUsed = 1, ClosedByUser = 2,
		Aborted = 3, ConnectionReset = 4, ConnectFailed = 6,
	};

	struct TcpConnection {
		SOCKET sock = OPENMSX_INVALID_SOCKET;
		std::atomic<uint8_t> tcpState{TCP_CLOSED};
		CloseReason closeReason = CloseReason::NeverUsed;
		bool     resident = false;
		uint32_t remoteIp = 0;
		uint16_t remotePort = 0;
		uint16_t localPort = 0;
		bool     connecting = false;
		std::deque<uint8_t> recvBuf;
		std::mutex mutex;
	};
	TcpConnection tcp[MAX_TCP]; // handles 1..MAX_TCP

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
		std::deque<UdpDatagram> recvQueue;
		std::mutex mutex;
	};
	UdpConnection udp[MAX_UDP];

	// --- ICMP echo reply queue ---
	struct IcmpReply {
		uint32_t srcIp = 0;
		uint8_t  ttl = 0;
		uint16_t identifier = 0;
		uint16_t sequence = 0;
		uint16_t dataLen = 0;
	};
	std::deque<IcmpReply> icmpReplies;
	std::mutex icmpMutex;
	std::thread icmpWorker;
	std::atomic<bool> icmpPending{false};
	// ICMP request for worker to handle
	struct IcmpRequest {
		uint32_t dstIp;
		uint8_t  ttl;
		uint16_t identifier;
		uint16_t sequence;
		uint16_t dataLen;
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
	void setResult(const uint8_t* data, size_t len);
	void setResultByte(uint8_t b);
	void setError();

	// Serialize a wire-layout struct (an Endian::UA_* record from
	// UnapiNetWire.hh) straight into the result buffer, replacing manual
	// byte packing. The compiler lays out the exact on-wire bytes.
	template<wire_layout T>
	void setResult(const T& d)
	{
		auto bytes = toBytes(d);
		setResult(bytes.data(), bytes.size());
	}
	// Fixed-size header struct followed by a variable payload
	// (used by TCP_RECV / UDP_RECV).
	template<wire_layout T>
	void setResult(const T& hdr, std::span<const uint8_t> payload)
	{
		auto h = toBytes(hdr);
		resultBuf.assign(h.begin(), h.end());
		resultBuf.insert(resultBuf.end(), payload.begin(), payload.end());
		resultPos = 0;
		state     = State::RESULT_READY;
		statusReg = STATUS_DATA;
	}

	[[nodiscard]] int allocTcpHandle();
	// Validate a 1-based handle and return the connection, or nullptr.
	[[nodiscard]] TcpConnection* tcpForHandle(int h);
	void closeTcpSocket(int h);
	// Quick teardown of a live connection from the receiver/send paths: record
	// the reason, close the socket and mark it CLOSED (does not clear the
	// remote/local metadata — that is closeTcpSocket's job).
	void forceClose(TcpConnection& c, CloseReason reason);
	[[nodiscard]] int allocUdpHandle();
	[[nodiscard]] UdpConnection* udpForHandle(int h);
	void closeUdpSocket(int h);
	void closeAllConnections();

};

} // namespace openmsx

#endif // UNAPINET_HH
