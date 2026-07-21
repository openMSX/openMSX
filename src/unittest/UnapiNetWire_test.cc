#include "catch.hpp"

#include "UnapiNetWire.hh"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace openmsx;

// Compile-time guards: the concept accepts our wire structs and rejects unsafe
// types (padding, containers, floats).
static_assert(wire_layout<TcpStateResult>);
static_assert(wire_layout<TcpOpenParams>);
static_assert(wire_layout<UdpRecvResultHeader>);
static_assert(wire_layout<IcmpRecvResult>);
namespace {
struct Padded { uint8_t a; uint32_t b; }; // has padding -> not unique object rep
}
static_assert(!wire_layout<Padded>);
static_assert(!wire_layout<float>);
static_assert(!wire_layout<std::vector<uint8_t>>);

[[nodiscard]] static std::vector<uint8_t> vec(std::span<const uint8_t> s)
{
	return {s.begin(), s.end()};
}

TEST_CASE("UnapiNetWire: TCP_STATE result is byte-identical (mixed endian)")
{
	TcpStateResult r{};
	r.state       = 0x04;        // TCP_ESTABLISHED
	r.avail       = 0x0102;
	r.closeReason = 0x01;
	r.remoteIp    = 0xC0A80101;  // 192.168.1.1
	r.remotePort  = 0x1F90;      // 8080
	r.localPort   = 0x0050;      // 80

	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{
		0x04, 0x02, 0x01, 0x01, 0xC0, 0xA8, 0x01, 0x01, 0x90, 0x1F, 0x50, 0x00});
}

TEST_CASE("UnapiNetWire: GET_LOCALIP result is 4-byte big-endian")
{
	GetLocalIpResult r{};
	r.ip = 0xC0A80101;
	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{0xC0, 0xA8, 0x01, 0x01});
}

TEST_CASE("UnapiNetWire: UDP_STATE result is little-endian size")
{
	UdpStateResult r{};
	r.firstDgramSize = 0x0004;
	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{0x04, 0x00});
}

TEST_CASE("UnapiNetWire: DNS_STATUS complete = status + big-endian IP")
{
	DnsStatusResult r{};
	r.status = 0x02;
	r.ip     = 0x08080808;       // 8.8.8.8
	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{0x02, 0x08, 0x08, 0x08, 0x08});
}

TEST_CASE("UnapiNetWire: UDP_RECV header (IP big, port/len little)")
{
	UdpRecvResultHeader r{};
	r.srcIp     = 0xC0A80101;
	r.srcPort   = 0x0035;        // 53
	r.actualLen = 0x0004;
	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{
		0xC0, 0xA8, 0x01, 0x01, 0x35, 0x00, 0x04, 0x00});
}

TEST_CASE("UnapiNetWire: ICMP_RECV record (12 bytes, mixed endian)")
{
	IcmpRecvResult r{};
	r.hasData    = 0x01;
	r.srcIp      = 0x08080808;
	r.ttl        = 0x40;
	r.identifier = 0x1234;
	r.sequence   = 0x0001;
	r.dataLen    = 0x0020;
	CHECK(vec(toBytes(r)) == std::vector<uint8_t>{
		0x01, 0x08, 0x08, 0x08, 0x08, 0x40, 0x34, 0x12, 0x01, 0x00, 0x20, 0x00});
}

TEST_CASE("UnapiNetWire: TCP_OPEN params round-trip (IP big, ports little)")
{
	std::array<uint8_t, 11> bytes{
		0xC0, 0xA8, 0x01, 0x01, 0x90, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x02};
	auto p = fromBytes<TcpOpenParams>(bytes);
	CHECK(uint32_t(p.remoteIp)   == 0xC0A80101);
	CHECK(uint16_t(p.remotePort) == 0x1F90);
	CHECK(uint16_t(p.localPort)  == 0x0000);
	CHECK(uint16_t(p.timeout)    == 0x0000);
	CHECK(p.flags                == 0x02);
}

TEST_CASE("UnapiNetWire: UDP_SEND param header round-trip")
{
	std::array<uint8_t, 9> bytes{
		0x01, 0xC0, 0xA8, 0x01, 0xFF, 0x99, 0x1F, 0x03, 0x00};
	auto p = fromBytes<UdpSendParamHeader>(bytes);
	CHECK(p.handle             == 0x01);
	CHECK(uint32_t(p.destIp)   == 0xC0A801FF);
	CHECK(uint16_t(p.destPort) == 0x1F99); // 8089
	CHECK(uint16_t(p.len)      == 0x0003);
}
