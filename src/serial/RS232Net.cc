#include "RS232Net.hh"
#include "RS232Connector.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
#include "checked_cast.hh"
#include "narrow.hh"
#include "Socket.hh"
#include "serialize.hh"
#include "ranges.hh"
#include "StringOp.hh"
#include <array>
#ifndef _WIN32
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif
// #include <sys/select.h>

namespace openmsx {

// IP232 protocol
static constexpr char IP232MAGIC    = '\xff';
/* sending */
static constexpr char IP232DTRLO    = '\x00';
static constexpr char IP232DTRHI    = '\x01';
/* reading */
static constexpr char IP232DCDLO    = '\x00';
static constexpr char IP232DCDHI    = '\x01';
static constexpr char IP232DCDMASK  = '\x01';

static constexpr char IP232RILO     = '\x00';
static constexpr char IP232RIHI     = '\x02';
static constexpr char IP232RIMASK   = '\x02';

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

// Pluggable
void RS232Net::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	if (!network_address_generate()){
		throw PlugException("Incorrect address / could not resolve: ", rs232NetAddressSetting.getString());
	}
	// Open socket client
	network_client();
	if (sockfd == OPENMSX_INVALID_SOCKET) {
		throw PlugException("Can't open connection");
	}

	DTR = false;
	RTS = true;
	DCD = false;
	RI  = false;
	IP232 = rs232NetUseIP232.getBoolean();

	auto& rs232Connector = checked_cast<RS232Connector&>(connector_);
	rs232Connector.setDataBits(SerialDataInterface::DATA_8);	// 8 data bits
	rs232Connector.setStopBits(SerialDataInterface::STOP_1);	// 1 stop bit
	rs232Connector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread = std::thread([this]() { run(); });
}

void RS232Net::unplugHelper(EmuTime::param /*time*/)
{
	// close socket
	if (sockfd != OPENMSX_INVALID_SOCKET){
		if (IP232){
			net_putc(IP232MAGIC);
			net_putc(IP232DTRLO);
		}
		sock_close(sockfd);
		sockfd = OPENMSX_INVALID_SOCKET;
    	}
	// input
	poller.abort();
	thread.join();
}

std::string_view RS232Net::getName() const
{
	return "rs232-net";
}

std::string_view RS232Net::getDescription() const
{
	return	"RS232 Network pluggable. Connects the RS232 port to IP:PORT, selected"
		" with the 'rs232-net-address' setting.";
}

void RS232Net::run()
{
	bool ipmagic = false;

	while (1){

		if ((poller.aborted()) || (sockfd == OPENMSX_INVALID_SOCKET)) {
			break;
		}

		// char b;
		auto b = net_getc();

		if (!b) continue;
		if (IP232) {
        		if (!ipmagic) {
				if (*b == IP232MAGIC) {
					ipmagic = true;
                    			/* literal 0xff */
                    			continue;
                		}
            		} else {
                		ipmagic = false;
				if (*b != IP232MAGIC) {
        				DCD = (*b & IP232DCDMASK) == IP232DCDHI;
               				// RI implemented in TCPSer 1.1.5 (not yet released) 
               				// RI present at least on Sony HBI-232 and HB-G900AP (bit 1 of &H82/&HBFFA status register)
               				//    missing on SVI-738
               				RI = (*b & IP232RIMASK) == IP232RIHI;
                   			continue;
               			}
        		}
		}


		assert(isPluggedIn());
		std::lock_guard<std::mutex> lock(mutex);
		queue.push_back(b.value());
		eventDistributor.distributeEvent(Rs232NetEvent());
	}
}

// input
void RS232Net::signal(EmuTime::param time)
{
	auto* conn = checked_cast<RS232Connector*>(getConnector());

	if (!conn->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}

	if ((!conn->ready())||(!RTS)) return;

	std::lock_guard<std::mutex> lock(mutex);
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
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
	}
	return 0;
}


// output
void RS232Net::recvByte(uint8_t value, EmuTime::param /*time*/)
{
	if ((value == 0xff) && (IP232)) {	// value matches IP232 magic
		net_putc(IP232MAGIC);
	}
	net_putc(value);
}

// Control lines

bool RS232Net::getDCD(EmuTime::param /*time*/) const
{
	return DCD;
}

void RS232Net::setDTR(bool status, EmuTime::param /*time*/)
{
	if (DTR != status) {
		DTR = status;
		if (IP232) {
			if (sockfd != OPENMSX_INVALID_SOCKET) {
				net_putc(IP232MAGIC);
				net_putc((DTR?IP232DTRHI:IP232DTRLO));
			}
		}
	}
}

void RS232Net::setRTS(bool status, EmuTime::param /*time*/)
{
	if (RTS != status) {
		RTS = status;
		std::lock_guard<std::mutex> lock(mutex);
		if ((RTS)&&(!queue.empty())){
			eventDistributor.distributeEvent(Rs232NetEvent());
		}
	}
}

// Socket routines below based on VICE emulator socket.c


bool RS232Net::net_putc(char b)
{

	if (sockfd == OPENMSX_INVALID_SOCKET)
	{
		return false;
	}

	auto n = sock_send(sockfd, &b, 1);
	if (n < 0) {
		sock_close(sockfd);
		sockfd = OPENMSX_INVALID_SOCKET;
		return false;
	}

	return true;
}

std::optional<char> RS232Net::net_getc()
{
	// int ret;
	// ptrdiff_t no_of_read_byte = -1;

	if (sockfd == OPENMSX_INVALID_SOCKET)  return {};


	if (selectPoll(sockfd) > 0) {
		char b;
		if (sock_recv(sockfd, &b, 1) !=1){
			sock_close(sockfd);
			sockfd = OPENMSX_INVALID_SOCKET;
			return {};
		} else return b;
	}
	return {};
}

// Check if the socket has incoming data to receive
// Returns: 1 if the specified socket has data; 0 if it does not contain
// any data, and -1 in case of an error.
int RS232Net::selectPoll(SOCKET readsock)
{
	struct timeval timeout = { 0, 0 };

	fd_set sdsocket;

	FD_ZERO(&sdsocket);
	FD_SET(readsock, &sdsocket);

	return select( readsock + 1, &sdsocket, nullptr, nullptr, &timeout);
}

// Open a socket and initialise it for client operation
//
//      The socket_address variable determines the type of
//      socket to be used (IPv4, IPv6, Unix Domain Socket, ...)

void RS232Net::network_client()
{

	sockfd = socket(socket_address.domain, SOCK_STREAM, socket_address.protocol);

	if (sockfd != OPENMSX_INVALID_SOCKET) {
		int one = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)(&one), sizeof(one));

		if (connect(sockfd, &socket_address.address.generic, socket_address.len) < 0) {
			sock_close(sockfd);
			sockfd = OPENMSX_INVALID_SOCKET;
		}
	}
}

// Initialise a socket address structure
void RS232Net::initialize_socket_address()
{
	memset(&socket_address, 0, sizeof(socket_address));
	socket_address.used = 1;
	socket_address.len = sizeof(socket_address.address);
}

// Generate a socket address
// 
//   Initialises a socket address with an IPv6 or IPv4 address.
// 
//   \param address_string
//      A string describing the address to set.
// 
//   return
//      true on success,
//      else an error occurred.
// 
//      address_string must be specified in one of the forms
//      <host>,
//      <host>:port
//      or
//      [<host>]:<port>
//      with <hostname> being the name of the host,
//      <hostipv6> being the IP of the host, and
//      <host> being the name of the host or its IPv6,
//      and <port> being the port number.
//
//      The extra braces [...] in case the port is specified are
//      needed as IPv6 addresses themselves already contain colons,
//      and it would be impossible to clearly distinguish between
//      an IPv6 address, and an IPv6 address where a port has been
//      specified. This format is a common one.
//
//
bool RS232Net::network_address_generate()
{
        std::string_view address = rs232NetAddressSetting.getString();
        if (address.empty()) {
                // there was no address given, do not try to process it.
                return false;
        }

        // preset the socket address
        memset(&socket_address.address, 0, sizeof(socket_address.address));
        socket_address.protocol = IPPROTO_TCP;

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        auto setIPv4 = [&] {
                hints.ai_family = AF_INET;
                socket_address.domain = PF_INET;
                socket_address.len = sizeof(socket_address.address.ipv4);
                socket_address.address.ipv4.sin_family = AF_INET;
                socket_address.address.ipv4.sin_port = 0;
                socket_address.address.ipv4.sin_addr.s_addr = INADDR_ANY;
        };
        auto setIPv6 = [&] {
                hints.ai_family = AF_INET6;
                socket_address.domain = PF_INET6;
                socket_address.len = sizeof(socket_address.address.ipv6);
                socket_address.address.ipv6.sin6_family = AF_INET6;
                socket_address.address.ipv6.sin6_port = 0;
                socket_address.address.ipv6.sin6_addr = in6addr_any;
        };

        // Parse 'address', fills in 'addressPart' and 'portPart'
        std::string addressPart;
        std::string portPart;
        auto [ipv6_address_part, ipv6_port_part] = StringOp::splitOnFirst(address, ']');
        if (!ipv6_port_part.empty()) { // there was a ']' character (and it's already dropped)
                // Try to parse as: "[IPv6]:port"
                if (!ipv6_address_part.starts_with('[') || !ipv6_port_part.starts_with(':')) {
                        // malformed address, do not try to process it.
                        return false;
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
                return false;
        }

        memcpy(&socket_address.address, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        return true;
}


template<typename Archive>
void RS232Net::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(RS232Net);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Net, "RS232Net");

} // namespace openmsx
