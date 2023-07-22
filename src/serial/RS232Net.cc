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
#include <array>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
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
    if (network_address_generate()){
        throw PlugException("Bad device name.  Should be ipaddr:port, but is ", rs232NetAddressSetting.getString());
    }
    // Open socket client
    network_client();
    if (sockfd < 0) {
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
    if (sockfd >= 0){
        if (IP232){
            net_putc(IP232MAGIC);
            net_putc(IP232DTRLO);
        }
        sock_close(sockfd);
        sockfd = INVALID_SOCKET;
    }
	// input
	poller.abort();
	thread.join();
}

std::string_view RS232Net::getName() const
{
	return "rs232-socket";
}

std::string_view RS232Net::getDescription() const
{
	return	"RS232 Network pluggable. Connects the RS232 port to IP:PORT, selected"
		" with the 'rs232-net-address' setting.";
}

void RS232Net::run()
{
    ptrdiff_t ret = -1;
    char b;
    bool ipmagic = false;

    while (1){

		if ((poller.aborted()) || (sockfd < 0)) {
			break;
		}

        // if(!RTS) continue;

        ret = net_getc(&b);

        if ((IP232) && (ret > 0)) {
            if (!ipmagic) {
                if (b == IP232MAGIC) {
                    ipmagic = true;
                    /* literal 0xff */
                    continue;
                }
            } else {
                ipmagic = false;
                if (b != IP232MAGIC) {
                    DCD = (b & IP232DCDMASK) == IP232DCDHI ? true : false;
                    // RI implemented in TCPSer 1.1.5 (not yet released) 
                    // RI present at least on Sony HBI-232 and HB-G900AP (bit 1 of &H82/&HBFFA status register)
                    //    missing on SVI-738
                    RI = (b & IP232RIMASK) == IP232RIHI ? true : false;
                    continue;
                }
            }
        }

        if (ret != 1) continue;

        // conn->recvByte(t, time);

		assert(isPluggedIn());
		std::lock_guard<std::mutex> lock(mutex);
		queue.push_back(b);
		eventDistributor.distributeEvent(
			Event::create<Rs232NetEvent>());
	}
}

// input
void RS232Net::signal(EmuTime::param time)
{
	auto* conn = checked_cast<RS232Connector*>(getConnector());

    char b;

	if (!conn->acceptsData()) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		return;
	}

	if ((!conn->ready())||(!RTS)) return;

	std::lock_guard<std::mutex> lock(mutex);
	if (queue.empty()) return;
    b = queue.pop_front();
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
			if (sockfd >= 0) {
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
            eventDistributor.distributeEvent(
			Event::create<Rs232NetEvent>());

        }
	}
}

// Socket routines below based on VICE emulator socket.c


int RS232Net::net_putc(uint8_t b)
{
    ptrdiff_t n;

    if (sockfd < 0)
    {
        // fprintf(stderr,"Attempt to write to invalid socket fd:%d\n", sockfd);
        return -1;
    }

    n = sock_send(sockfd, reinterpret_cast<char *>(&b), 1);
    if (n < 0) {
        sock_close(sockfd);
        sockfd = INVALID_SOCKET;
        // fprintf(stderr,"Error writing to socket\n");
        return -1;
    }

    return 0;
}

ptrdiff_t RS232Net::net_getc(char * b)
{
    int ret;
    ptrdiff_t no_of_read_byte = -1;

    if (sockfd < 0) {
        // fprintf(stderr,"Attempt to read from invalid socket fd:%d\n", sockfd);
        return no_of_read_byte;
    }


    do {

        /* from now on, assume everything is ok,
            but we have not received any bytes */
        no_of_read_byte = 0;

        ret = selectPoll(sockfd);

        if (ret > 0) {
            no_of_read_byte = sock_recv(sockfd, b, 1);
            if ( no_of_read_byte != 1 ) {
                sock_close(sockfd);
                sockfd = INVALID_SOCKET;
                // if ( no_of_read_byte < 0 ) {
                    // fprintf(stderr,"Error reading socket\n");
                // } else {
                    // fprintf(stderr,"EOF\n");
                // }
            }
        }
    } while (0);
    return no_of_read_byte;
}

// Check if the socket has incoming data to receive
// Returns: 1 if the specified socket has data; 0 if it does not contain
// any data, and -1 in case of an error.
int RS232Net::selectPoll(SOCKET readsock)
{
	TIMEVAL timeout = { 0, 0 };

	fd_set sdsocket;

	FD_ZERO(&sdsocket);
	FD_SET(readsock, &sdsocket);

	return select( readsock + 1, &sdsocket, NULL, NULL, &timeout);
}

// Open a socket and initialise it for client operation
//
//      The socket_address variable determines the type of
//      socket to be used (IPv4, IPv6, Unix Domain Socket, ...)

void RS232Net::network_client()
{
    sockfd = INVALID_SOCKET;
    int error = 1;

    do {

        sockfd = socket(socket_address.domain, SOCK_STREAM, socket_address.protocol);

        if (sockfd == INVALID_SOCKET) {
            break;
        }

// #if defined(TCP_NODELAY)
        setsockopt(sockfd, SOL_TCP, TCP_NODELAY, static_cast<const void*>(&error), sizeof(error)); /* just an integer with 1, not really an error */
// #endif

        if (connect(sockfd, &socket_address.address.generic, socket_address.len) < 0) {
            break;
        }
        error = 0;
    } while (0);

    if (error) {
        if (sockfd != INVALID_SOCKET) {
            sock_close(sockfd);
        }
        sockfd = INVALID_SOCKET;
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
int RS232Net::network_address_generate()
{

	auto address_string = rs232NetAddressSetting.getString();

    int error = 1;

    do {
        if (address_string.data() && strncmp("ip6://", address_string.data(), sizeof("ip6://" - 1)) == 0) {
            if (network_address_generate_ipv6(&address_string.data()[sizeof("ip6://" - 1)])) {
                break;
            }
        } else if (address_string.data() && strncmp("ip4://", address_string.data(), sizeof("ip4://" - 1)) == 0) {
            if (network_address_generate_ipv4(&address_string.data()[sizeof("ip4://" - 1)])) {
                break;
            }
        } else {
            /* the user did not specify the type of the address, try to guess it by trying IPv6, then IPv4 */
// #ifdef HAVE_IPV6
            if (network_address_generate_ipv6(address_string.data()))
// #endif /* #ifdef HAVE_IPV6 */
            {
                if (network_address_generate_ipv4(address_string.data())) {
                    break;
                }
            }
        }
        error = 0;
    } while (0);
    return error;
}

// Generate an IPv4 socket address
//   Initialises a socket address with an IPv4 address.
//
//   \param address_string
//      A string describing the address to set.
//
//   returns:
//      0 on success,
//      else an error occurred.
//
//   \remark
//      address_string must be specified in the form
//      <host>
//      or
//      <host>:<port>
//      with <host> being the name or the IPv4 address of the host,
//      and <port< being the port number. If the first form is used,
//      the default port will be used.
int RS232Net::network_address_generate_ipv4(const char *address_string)
{
    char * address_part = strdup(address_string);
    int error = 1;

    do {
        /* preset the socket address with port and INADDR_ANY */

        memset(&socket_address.address, 0, sizeof(socket_address.address));
        socket_address.domain = PF_INET;
        socket_address.protocol = IPPROTO_TCP;
        socket_address.len = sizeof(socket_address.address.ipv4);
        socket_address.address.ipv4.sin_family = AF_INET;
        socket_address.address.ipv4.sin_port = htons(0);
        socket_address.address.ipv4.sin_addr.s_addr = INADDR_ANY;

        if (address_string) {
            /* an address string was specified, try to use it */
            struct hostent * host_entry;

            const char * port_part = NULL;

            /* try to find out if a port has been specified */
            port_part = strchr(address_string, ':');

            if (port_part) {
                char * p;
                unsigned long new_port;

                /* yes, there is a port: Copy the part before, so we can modify it */
                p = strdup(address_string);

                p[port_part - address_string] = 0;
                free(address_part);
                address_part = p;

                ++port_part;

                new_port = strtoul(port_part, &p, 10);

                if (*p == 0) {
                    socket_address.address.ipv4.sin_port = htons(static_cast<unsigned short>(new_port));
                }
            }

            if (address_part[0] == 0) {
                /* there was no address give, do not try to process it. */
                error = 0;
                break;
            }

            host_entry = gethostbyname(address_part);

            if (host_entry != NULL && host_entry->h_addrtype == AF_INET) {
                if (host_entry->h_length != sizeof socket_address.address.ipv4.sin_addr.s_addr) {
                    /* something weird happened... SHOULD NOT HAPPEN! */
					throw PlugException("gethostbyname() returned an IPv4 address, but the length is wrong: ", host_entry->h_length );
                    break;
                }

                memcpy(&socket_address.address.ipv4.sin_addr.s_addr,
                        host_entry->h_addr_list[0],
                        host_entry->h_length);
            } else {
                /* Assume it is an IP address */

                if (address_part[0] != 0) {
// #ifdef HAVE_INET_ATON
//                     /*
//                      * this implementation is preferable, as inet_addr() cannot
//                      * return the broadcast address, as it has the same encoding
//                      * as INADDR_NONE.
//                      *
//                      * Unfortunately, not all ports have inet_aton(), so, we must
//                      * provide both implementations.
//                      */
//                     if (inet_aton(address_part,
//                                 &socket_address->address.ipv4.sin_addr.s_addr) == 0) {
//                         /* no valid IP address */
//                         break;
//                     }
// #else
                    in_addr_t ip = inet_addr(address_part);

                    if (ip == INADDR_NONE) {
                        /* no valid IP address */
                        break;
                    }
                    socket_address.address.ipv4.sin_addr.s_addr = ip;
// #endif
                }

            }

            error = 0;
        }
    } while (0);

    free(address_part);
    return error;
}

// Generate an IPv6 socket address
// 
//   Initialises a socket address with an IPv6 address.
// 
//   \param address_string
//      A string describing the address to set.
// 
//   return
//      0 on success,
//      else an error occurred.
// 
//      address_string must be specified in one of the forms
//      <host>
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
//      On platforms which do not support IPv6, this function
//      returns -1 as error.
//
int RS232Net::network_address_generate_ipv6(const char * address_string)
{
// #ifdef HAVE_IPV6
    int error = 1;

    do {
        struct hostent * host_entry = NULL;
// #ifndef HAVE_GETHOSTBYNAME2
//         int err6;
// #endif

        /* preset the socket address */

        memset(&socket_address.address, 0, sizeof socket_address.address);
        socket_address.domain = PF_INET6;
        socket_address.protocol = IPPROTO_TCP;
        socket_address.len = sizeof socket_address.address.ipv6;
        socket_address.address.ipv6.sin6_family = AF_INET6;
        socket_address.address.ipv6.sin6_port = 0;
        socket_address.address.ipv6.sin6_addr = in6addr_any;

        if (!address_string || address_string[0] == 0) {
            /* there was no address give, do not try to process it. */
            error = 0;
            break;
        }
// #ifdef HAVE_GETHOSTBYNAME2
        host_entry = gethostbyname2(address_string, AF_INET6);
// #else
//         host_entry = getipnodebyname(address_string, AF_INET6, AI_DEFAULT, &err6);
// #endif
        if (host_entry == NULL) {
            break;
        }

        memcpy(&socket_address.address.ipv6.sin6_addr, host_entry->h_addr, host_entry->h_length);

// #ifndef HAVE_GETHOSTBYNAME2
//         freehostent(host_entry);
// #endif
        error = 0;
    } while (0);

    return error;
// #else /* #ifdef HAVE_IPV6 */
//     log_message(LOG_DEFAULT, "IPv6 is not supported in this installation of VICE!\n");
//     return -1;
// #endif /* #ifdef HAVE_IPV6 */
}

template<typename Archive>
void RS232Net::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(RS232Net);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Net, "RS232Net");

} // namespace openmsx
