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
	return "rs232-net";
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
		eventDistributor.distributeEvent(Rs232NetEvent());
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
            eventDistributor.distributeEvent(Rs232NetEvent());

        }
	}
}

// Socket routines below based on VICE emulator socket.c


int RS232Net::net_putc(uint8_t b)
{
    ptrdiff_t n;

    if (sockfd < 0)
    {
        return -1;
    }

    n = sock_send(sockfd, reinterpret_cast<char *>(&b), 1);
    if (n < 0) {
        sock_close(sockfd);
        sockfd = INVALID_SOCKET;
        return -1;
    }

    return 0;
}

ptrdiff_t RS232Net::net_getc(char * b)
{
    int ret;
    ptrdiff_t no_of_read_byte = -1;

    if (sockfd < 0) {
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

#ifndef _WIN32
        setsockopt(sockfd, SOL_TCP, TCP_NODELAY, static_cast<const void*>(&error), sizeof(error)); /* just an integer with 1, not really an error */
#endif

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
        if (network_address_generate_sockaddr(address_string.data()))
        {
            break;
        }
        error = 0;
    } while (0);
    return error;
}

// Generate a socket address
// 
//   Initialises a socket address with an IPv6 or IPv4 address.
// 
//   \param address_string
//      A string describing the address to set.
// 
//   return
//      0 on success,
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
int RS232Net::network_address_generate_sockaddr(const char * address_string)
{
    char * address_part = strdup(address_string);
    int error = 1;

    do {
        struct addrinfo hints,*res;
        int err6;

        memset(&hints,0,sizeof(hints));

        /* preset the socket address */
        memset(&socket_address.address, 0, sizeof socket_address.address);
        socket_address.protocol = IPPROTO_TCP;

        if (!address_string || address_string[0] == 0) {
            /* there was no address given, do not try to process it. */
            error = 0;
            break;
        }
        const char * port_part = NULL;

        /* try to find out if a port has been specified */
        port_part = strchr(address_string, ']');
            
        if (port_part) {
            if ((address_string[0] == '[') && (port_part[1] == ':')) {
                /* [IPv6]:port format*/
                char * p;

                ++port_part;
                p = strdup(address_string+1);
                p[port_part - (address_string+2)] = 0;
                free(address_part);
                address_part = p;
                ++port_part;
                hints.ai_family = AF_INET6;
                socket_address.domain = PF_INET6;
                socket_address.len = sizeof socket_address.address.ipv6;
                socket_address.address.ipv6.sin6_family = AF_INET6;
                socket_address.address.ipv6.sin6_port = 0;
                socket_address.address.ipv6.sin6_addr = in6addr_any;
            } else {
                /* malformed address, do not try to process it. */
                break;
            }
        } else {
            /* count number of colons */
            int i = 0;
            const char *pch=strchr(address_string,':');
            while (pch!=NULL) {
                i++;
                pch=strchr(pch+1,':');
            }
            if (i == 1) {
                /* either domainname:port or ipv4:port */
                port_part = strchr(address_string,':');
                char * p;

                p = strdup(address_string);
                p[port_part - address_string] = 0;
                free(address_part);
                address_part = p;
                ++port_part;
                hints.ai_family = AF_INET;
                socket_address.domain = PF_INET;
                socket_address.len = sizeof socket_address.address.ipv4;
                socket_address.address.ipv4.sin_family = AF_INET;
                socket_address.address.ipv4.sin_port = 0;
                socket_address.address.ipv4.sin_addr.s_addr = INADDR_ANY;
            } else if (i > 1) {
                /* ipv6 address */
 
                hints.ai_family = AF_INET6;
                socket_address.domain = PF_INET6;
                socket_address.len = sizeof socket_address.address.ipv6;
                socket_address.address.ipv6.sin6_family = AF_INET6;
                socket_address.address.ipv6.sin6_port = 0;
                socket_address.address.ipv6.sin6_addr = in6addr_any;
            }
        }

        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        err6 = getaddrinfo(address_part,port_part,&hints,&res);
        if ( err6 != 0) {
            break;
        }
        memcpy(&socket_address.address, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        error = 0;
    } while (0);

    free(address_part);
    return error;
}

template<typename Archive>
void RS232Net::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(RS232Net);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Net, "RS232Net");

} // namespace openmsx
