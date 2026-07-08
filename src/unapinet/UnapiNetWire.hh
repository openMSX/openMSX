#ifndef UNAPINETWIRE_HH
#define UNAPINETWIRE_HH

#include "endian.hh"

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

// UNAPI bridge wire layouts (openMSX Endian::UA_* unaligned types).
//
// The MSX <-> host bridge exchanges fixed byte records over I/O ports 0x28/0x29.
// Instead of hand-shuffling bytes with shifts and masks, we describe each record
// once as a struct whose field types encode the on-wire endianness, and let the
// compiler produce the exact bytes. Type mapping (verified against both the
// current C++ and the Z80 TSR msx/unapinet.asm):
//   * IPv4 address (network/big-endian octets a,b,c,d) .......... Endian::UA_B32
//   * 16-bit port / length / size / id / seq (little-endian) .... Endian::UA_L16
//   * single byte (status, handle, flags, ttl, magic, state) .... uint8_t
//   * verbatim variable payload ................................. appended
//     separately (NOT a struct member).
//
// All UA_* members have alignof==1, so each struct is naturally packed (no
// interior padding); the static_assert on each size guards that.
//
// USAGE (UA_* has operator= only, no int constructor):
//   result:  TcpStateResult r{}; r.state = ...; r.remoteIp = ...; setResult(r);
//   param:   auto p = fromBytes<TcpOpenParams>(paramBuf); uint32_t ip = p.remoteIp;

namespace openmsx {

// A type whose in-memory bytes ARE its wire representation: trivially copyable,
// standard layout, and free of padding bits (every byte meaningful). Accepts
// uint8_t + Endian::UA_* aggregates; rejects padded structs, floats, containers.
template<typename T>
concept wire_layout =
	std::is_trivially_copyable_v<T> &&
	std::is_standard_layout_v<T> &&
	std::has_unique_object_representations_v<T>;

// View a wire-layout value as its exact on-wire bytes (no copy). The result
// aliases the argument (char-pointer aliasing is well-defined), so it must
// not outlive it.
template<wire_layout T>
[[nodiscard]] std::span<const uint8_t, sizeof(T)> asBytes(const T& d)
{
	return std::span<const uint8_t, sizeof(T)>(
		reinterpret_cast<const uint8_t*>(&d), sizeof(T));
}

// Copy a wire-layout value to its exact on-wire bytes.
template<wire_layout T>
[[nodiscard]] std::array<uint8_t, sizeof(T)> toBytes(const T& d)
{
	std::array<uint8_t, sizeof(T)> out;
	std::memcpy(out.data(), &d, sizeof(T));
	return out;
}

// Deserialize a wire-layout value from a byte span (caller guarantees size).
template<wire_layout T>
[[nodiscard]] T fromBytes(std::span<const uint8_t> bytes)
{
	assert(bytes.size() >= sizeof(T));
	T d;
	std::memcpy(&d, bytes.data(), sizeof(T));
	return d;
}

// ---- CMD_QUERY_CAP (0x10) : 2 fixed bytes --------------------------------
struct QueryCapResult {
	uint8_t cap0;
	uint8_t cap1;
};
static_assert(sizeof(QueryCapResult) == 2);

// ---- CMD_DNS_QUERY (0x01) result, direct-IP-literal path : status + IP ---
struct DnsQueryResult {
	uint8_t        status;   // = 1 (resolved immediately); value 2 never emitted here
	Endian::UA_B32 ip;       // big-endian a.b.c.d
};
static_assert(sizeof(DnsQueryResult) == 5);

// ---- CMD_DNS_STATUS (0x02) result ----------------------------------------
struct DnsStatusResult {     // path A: complete
	uint8_t        status;   // = 2
	Endian::UA_B32 ip;       // big-endian
};
static_assert(sizeof(DnsStatusResult) == 5);
struct DnsStatusError {      // path B: error
	uint8_t status;          // = 0xFF
	uint8_t errorCode;
};
static_assert(sizeof(DnsStatusError) == 2);

// ---- CMD_TCP_OPEN (0x03) param : 11 bytes copied verbatim by the TSR -----
struct TcpOpenParams {
	Endian::UA_B32 remoteIp;    // big-endian
	Endian::UA_L16 remotePort;  // little-endian
	Endian::UA_L16 localPort;   // little-endian
	Endian::UA_L16 timeout;     // little-endian -- NEVER read by C++, reserved so
	uint8_t        flags;       //   'flags' stays at offset 10 (bit0=passive,bit1=resident)
};
static_assert(sizeof(TcpOpenParams) == 11);

// ---- CMD_TCP_SEND (0x04) param header (+ 'len' payload bytes) -------------
struct TcpSendParamHeader {
	uint8_t        handle;
	Endian::UA_L16 len;         // little-endian
};
static_assert(sizeof(TcpSendParamHeader) == 3);

// ---- CMD_TCP_RECV (0x05) -------------------------------------------------
struct TcpRecvParams {
	uint8_t        handle;
	Endian::UA_L16 maxlen;      // little-endian; clamped to MAX_TRANSFER
};
static_assert(sizeof(TcpRecvParams) == 3);
struct TcpRecvResultHeader {
	Endian::UA_L16 actualLen;   // little-endian; then 'actualLen' payload bytes
};
static_assert(sizeof(TcpRecvResultHeader) == 2);

// ---- CMD_TCP_STATE (0x07) result : always 12 bytes, MIXED endian ---------
struct TcpStateResult {
	uint8_t        state;
	Endian::UA_L16 avail;       // little-endian
	uint8_t        closeReason;
	Endian::UA_B32 remoteIp;    // big-endian
	Endian::UA_L16 remotePort;  // little-endian
	Endian::UA_L16 localPort;   // little-endian
};
static_assert(sizeof(TcpStateResult) == 12);

// ---- CMD_GET_LOCALIP (0x0D) result : 4-byte big-endian IP ----------------
struct GetLocalIpResult {
	Endian::UA_B32 ip;          // big-endian a.b.c.d
};
static_assert(sizeof(GetLocalIpResult) == 4);

// ---- CMD_UDP_OPEN (0x09) param -------------------------------------------
struct UdpOpenParams {
	Endian::UA_L16 localPort;   // little-endian; 0xFFFF = ephemeral
};
static_assert(sizeof(UdpOpenParams) == 2);

// ---- CMD_UDP_STATE (0x0B) result -----------------------------------------
struct UdpStateResult {
	Endian::UA_L16 firstDgramSize; // little-endian; 0 = none
};
static_assert(sizeof(UdpStateResult) == 2);

// ---- CMD_UDP_SEND (0x0C) param header (+ 'len' payload bytes) ------------
struct UdpSendParamHeader {
	uint8_t        handle;
	Endian::UA_B32 destIp;      // big-endian
	Endian::UA_L16 destPort;    // little-endian
	Endian::UA_L16 len;         // little-endian
};
static_assert(sizeof(UdpSendParamHeader) == 9);

// ---- CMD_UDP_RECV (0x0F) : 8-byte header, MIXED endian (+ payload) --------
struct UdpRecvParams {
	uint8_t        handle;
	Endian::UA_L16 maxlen;      // little-endian
};
static_assert(sizeof(UdpRecvParams) == 3);
struct UdpRecvResultHeader {
	Endian::UA_B32 srcIp;       // big-endian
	Endian::UA_L16 srcPort;     // little-endian
	Endian::UA_L16 actualLen;   // little-endian; then 'actualLen' payload bytes
};
static_assert(sizeof(UdpRecvResultHeader) == 8);

// ---- CMD_ICMP_SEND (0x11) param : 11 bytes copied verbatim ---------------
struct IcmpSendParams {
	Endian::UA_B32 dstIp;       // big-endian
	uint8_t        ttl;
	Endian::UA_L16 identifier;  // little-endian
	Endian::UA_L16 sequence;    // little-endian
	Endian::UA_L16 len;         // little-endian; clamped to 512 (size, not payload)
};
static_assert(sizeof(IcmpSendParams) == 11);

// ---- CMD_ICMP_RECV (0x12) result : data-present path, 12 bytes -----------
struct IcmpRecvResult {
	uint8_t        hasData;     // = 1
	Endian::UA_B32 srcIp;       // big-endian
	uint8_t        ttl;
	Endian::UA_L16 identifier;  // little-endian
	Endian::UA_L16 sequence;    // little-endian
	Endian::UA_L16 dataLen;     // little-endian
};
static_assert(sizeof(IcmpRecvResult) == 12);

} // namespace openmsx

#endif // UNAPINETWIRE_HH
