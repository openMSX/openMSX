#include "RS232Net.hh"

#include "RS232Connector.hh"

#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "serialize.hh"

#include "checked_cast.hh"
#include "ranges.hh"
#include "StringOp.hh"

#include <array>
#include <cassert>
#ifndef _WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif

namespace openmsx {

// IP232 protocol
static constexpr char IP232_MAGIC    = '\xff';

// sending
static constexpr char IP232_DTR_LO   = '\x00';
static constexpr char IP232_DTR_HI   = '\x01';

// receiving
static constexpr char IP232_DCD_LO   = '\x00';
static constexpr char IP232_DCD_HI   = '\x01';
static constexpr char IP232_DCD_MASK = '\x01';

static constexpr char IP232_RI_LO    = '\x00';
static constexpr char IP232_RI_HI    = '\x02';
static constexpr char IP232_RI_MASK  = '\x02';

RS232Net::RS232Net(EventDistributor& eventDistributor_,
                         Scheduler& scheduler_,
                         CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, rs232NetAddressSetting(
	        commandController, "rs232-net-address",
	        "IP/address:port for RS232 net pluggable",
	        "127.0.0.1:25232")
	, rs232NetUseIP232(
	        commandController, "rs232-net-ip232",
	        "Enable IP232 protocol",
	        true)
{
	eventDistributor.registerEventListener(EventType::RS232_NET, *this);
}

RS232Net::~RS232Net()
{
	eventDistributor.unregisterEventListener(EventType::RS232_NET, *this);
}

// Parse a IPv6 or IPv4 address string.
//
// The address must be specified in one of the forms:
//   <host>
//   <host>:port
//   [<hostipv6>]:<port>
// with:
//   <hostname> being the name of the host,
//   <hostipv6> being the IP of the host, and
//   <host>     being the name of the host or its IPv6,
//   <port>     being the port number.
//
// The extra braces [...] in case the port is specified are needed as IPv6
// addresses themselves already contain colons, and it would be impossible to
// clearly distinguish between an IPv6 address, and an IPv6 address where a port
// has been specified. This format is a common one.
static std::optional<RS232Net::NetworkSocketAddress> parseNetworkAddress(std::string_view address)
{
	if (address.empty()) {
		// There was no address given, do not try to process it.
		return {};
	}

	// Preset the socket address
	RS232Net::NetworkSocketAddress result;
	memset(&result.address, 0, sizeof(result.address));

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	auto setIPv4 = [&] {
		hints.ai_family = AF_INET;
		result.domain = PF_INET;
		result.len = sizeof(result.address.ipv4);
		result.address.ipv4.sin_family = AF_INET;
		result.address.ipv4.sin_port = 0;
		result.address.ipv4.sin_addr.s_addr = INADDR_ANY;
	};
	auto setIPv6 = [&] {
		hints.ai_family = AF_INET6;
		result.domain = PF_INET6;
		result.len = sizeof(result.address.ipv6);
		result.address.ipv6.sin6_family = AF_INET6;
		result.address.ipv6.sin6_port = 0;
		result.address.ipv6.sin6_addr = in6addr_any;
	};

	// Parse 'address', fills in 'addressPart' and 'portPart'
	std::string addressPart;
	std::string portPart;
	auto [ipv6_address_part, ipv6_port_part] = StringOp::splitOnFirst(address, ']');
	if (!ipv6_port_part.empty()) { // there was a ']' character (and it's already dropped)
		// Try to parse as: "[IPv6]:port"
		if (!ipv6_address_part.starts_with('[') || !ipv6_port_part.starts_with(':')) {
			// Malformed address, do not try to process it.
			return {};
		}
		addressPart = std::string(ipv6_address_part.substr(1)); // drop '['
		portPart    = std::string(ipv6_port_part   .substr(1)); // drop ':'
		setIPv6();
	} else {
		auto numColons = ranges::count(address, ':');
		if (numColons == 0) {
			// either IPv4 or IPv6
			addressPart = std::string(address);
		} else if (numColons == 1) {
			// ipv4:port
			auto [ipv4_address_part, ipv4_port_part] = StringOp::splitOnFirst(address, ':');
			addressPart = std::string(ipv4_address_part);
			portPart    = std::string(ipv4_port_part);
			setIPv4();
		} else {
			// ipv6 address
			addressPart = std::string(address);
			setIPv6();
		}
	}

	// Interpret 'addressPart' and 'portPart' (possibly the latter is empty)
	struct addrinfo* res;
	if (getaddrinfo(addressPart.c_str(), portPart.c_str(), &hints, &res) != 0) {
		return {};
	}

	memcpy(&result.address, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	return result;
}

// Pluggable
void RS232Net::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	auto address = rs232NetAddressSetting.getString();
	auto socketAddress = parseNetworkAddress(address);
	if (!socketAddress) {
		throw PlugException("Incorrect address / could not resolve: ", address);
	}
	open_socket(*socketAddress);
	if (sockfd == OPENMSX_INVALID_SOCKET) {
		throw PlugException("Can't open connection");
	}

	DTR = false;
	RTS = true;
	DCD = false;
	RI  = false;
	IP232 = rs232NetUseIP232.getBoolean();

	auto& rs232Connector = checked_cast<RS232Connector&>(connector_);
	rs232Connector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	rs232Connector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	rs232Connector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	poller.reset();
	thread = std::thread([this]() { run(); });
}

void RS232Net::unplugHelper(EmuTime::param /*time*/)
{
	// close socket
	if (sockfd != OPENMSX_INVALID_SOCKET) {
		if (IP232) {
			static constexpr std::array<char, 2> dtr_lo = {IP232_MAGIC, IP232_DTR_LO};
			net_put(dtr_lo);
		}
		sock_close(sockfd);
		sockfd = OPENMSX_INVALID_SOCKET;
	}
	// stop helper thread
	poller.abort();
	if (thread.joinable()) thread.join();
}

std::string_view RS232Net::getName() const
{
	return "rs232-net";
}

std::string_view RS232Net::getDescription() const
{
	return "RS232 Network pluggable. Connects the RS232 port to IP:PORT, "
	       "selected with the 'rs232-net-address' setting.";
}

void RS232Net::run()
{
	bool ipMagic = false;
	while (true) {
		if (sockfd == OPENMSX_INVALID_SOCKET) break;
#ifndef _WIN32
		if (poller.poll(sockfd)) {
			break; // error or abort
		}
#endif
		char b;
		auto n = sock_recv(sockfd, &b, sizeof(b));
		if (n < 0) { // error
			sock_close(sockfd);
			sockfd = OPENMSX_INVALID_SOCKET;
			break;
		} else if (n == 0) { // no data, try again
			continue;
		}

		// Process received byte
		if (IP232) {
			if (ipMagic) {
				ipMagic = false;
				if (b != IP232_MAGIC) {
					DCD = (b & IP232_DCD_MASK) == IP232_DCD_HI;
					// RI implemented in TCPSer 1.1.5 (not yet released)
					// RI present at least on Sony HBI-232 and HB-G900AP (bit 1 of &H82/&HBFFA status register)
					//    missing on SVI-738
					RI = (b & IP232_RI_MASK) == IP232_RI_HI;
					continue;
				}
				// was a literal 0xff
			} else {
				if (b == IP232_MAGIC) {
					ipMagic = true;
					continue;
				}
			}
		}

		// Put received byte in queue and notify main emulation thread
		assert(isPluggedIn());
		{
			std::lock_guard lock(mutex);
			queue.push_back(b);
		}
		eventDistributor.distributeEvent(Rs232NetEvent());
	}
}

// input
void RS232Net::signal(EmuTime::param time)
{
	auto* conn = checked_cast<RS232Connector*>(getConnector());

	if (!conn->acceptsData()) {
		std::lock_guard lock(mutex);
		queue.clear();
		return;
	}

	if (!conn->ready() || !RTS) return;

	std::lock_guard lock(mutex);
	if (queue.empty()) return;
	char b = queue.pop_front();
	conn->recvByte(b, time);
}

// EventListener
int RS232Net::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::lock_guard lock(mutex);
		queue.clear();
	}
	return 0;
}

// output
void RS232Net::recvByte(uint8_t value_, EmuTime::param /*time*/)
{
	if (sockfd == OPENMSX_INVALID_SOCKET) return;

	auto value = static_cast<char>(value_);
	if ((value == IP232_MAGIC) && IP232) {
		static constexpr std::array<char, 2> ff = {IP232_MAGIC, IP232_MAGIC};
		net_put(ff);
	} else {
		net_put(std::span{&value, 1});
	}
}

// Control lines

std::optional<bool> RS232Net::getDSR(EmuTime::param /*time*/) const
{
	return true; // Needed to set this line in the correct state for a plugged device
}

std::optional<bool> RS232Net::getCTS(EmuTime::param /*time*/) const
{
	return true; // TODO: Implement when IP232 adds support for CTS
}

std::optional<bool> RS232Net::getDCD(EmuTime::param /*time*/) const
{
	return DCD;
}

std::optional<bool> RS232Net::getRI(EmuTime::param /*time*/) const
{
	return RI;
}

void RS232Net::setDTR(bool status, EmuTime::param /*time*/)
{
	if (DTR == status) return;
	DTR = status;

	if (sockfd == OPENMSX_INVALID_SOCKET) return;
	if (IP232) {
		std::array<char, 2> dtr = {IP232_MAGIC, DTR ? IP232_DTR_HI : IP232_DTR_LO};
		net_put(dtr);
	}
}

void RS232Net::setRTS(bool status, EmuTime::param /*time*/)
{
	if (RTS == status) return;
	RTS = status;
	if (RTS) {
		std::lock_guard lock(mutex);
		if (!queue.empty()) {
			eventDistributor.distributeEvent(Rs232NetEvent());
		}
	}
}

// Socket routines below based on VICE emulator socket.c
bool RS232Net::net_put(std::span<const char> buf)
{
	assert(sockfd != OPENMSX_INVALID_SOCKET);

	if (auto n = sock_send(sockfd, buf.data(), buf.size()); n < 0) {
		sock_close(sockfd);
		sockfd = OPENMSX_INVALID_SOCKET;
		return false;
	}
	return true;
}

// Open a socket and initialise it for client operation
void RS232Net::open_socket(const NetworkSocketAddress& socket_address)
{
	sockfd = socket(socket_address.domain, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == OPENMSX_INVALID_SOCKET) return;

	int one = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&one), sizeof(one));

	if (connect(sockfd, &socket_address.address.generic, socket_address.len) < 0) {
		sock_close(sockfd);
		sockfd = OPENMSX_INVALID_SOCKET;
	}
}

template<typename Archive>
void RS232Net::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(RS232Net);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Net, "RS232Net");

} // namespace openmsx
