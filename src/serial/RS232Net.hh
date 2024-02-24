#ifndef RS232SOCKET_HH
#define RS232SOCKET_HH

#include "RS232Device.hh"
#include "EventListener.hh"
// #include "FilenameSetting.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "FileOperations.hh"
#include "circular_buffer.hh"
#include "Poller.hh"
#include "Socket.hh"
#include <fstream>
#include <mutex>
#include <thread>
#include <cstdint>
#include <cstdio>

namespace openmsx {

#ifndef INVALID_SOCKET
# define INVALID_SOCKET -1
#endif

class EventDistributor;
class Scheduler;
class CommandController;

class RS232Net final : public RS232Device, private EventListener
{
public:
	RS232Net(EventDistributor& eventDistributor, Scheduler& scheduler,
	            CommandController& commandController);
	~RS232Net() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// input
	void signal(EmuTime::param time) override;

	// output
	void recvByte(uint8_t value, EmuTime::param time) override;

	[[nodiscard]] virtual bool getDCD(EmuTime::param time) const override;
	virtual void setDTR(bool status, EmuTime::param time);
	virtual void setRTS(bool status, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	typedef struct timeval TIMEVAL;

	bool DTR;	//Data Terminal Ready output status
	bool RTS;	//Request To Send output status
	bool DCD;	//Data Carrier Detect input status
	bool RI;	//Ring Indicator input status
	bool IP232;

	// EventListener
	int signalEvent(const Event& event) override;

	int net_putc(uint8_t b);

	ptrdiff_t net_getc(char * b);

	int selectPoll(SOCKET readsock);

	void network_client();
	
	void initialize_socket_address();

	int network_address_generate();

	int network_address_generate_sockaddr(const char *address_string);

private:

	// Access to socket addresses
	//
	// This union is used to access socket addresses.
	// It replaces the casts needed otherwise.
	//
	// Furthermore, it ensures we have always enough
	// memory for any type needed. (sizeof struct sockaddr_in6
	// is bigger than struct sockaddr).
	//
	// Note, however, that there is no consensus if the C standard
	// guarantees that all union members start at the same
	// address. There are arguments for and against this.
	//
	// However, in practice, this approach works.
	//
	union socket_addresses_u {
		struct sockaddr generic;     /*!< the generic type needed for calling the socket API */

	// #ifdef HAVE_UNIX_DOMAIN_SOCKETS
	#ifndef _WIN32
		struct sockaddr_un local;    /*!< an Unix Domain Socket (file system) socket address */
	#endif

		struct sockaddr_in ipv4;     /*!< an IPv4 socket address */

	// #ifdef HAVE_IPV6
		struct sockaddr_in6 ipv6;    /*!< an IPv6 socket address */
	// #endif
	};

	// opaque structure describing an address for use with socket functions
	struct rs232_network_socket_address_s {
		unsigned int used;      /*!< 1 if this entry is being used, 0 else.
								* This is used for debugging the buffer
								* allocation strategy.
								*/
		int domain;             /*!< the address family (AF_INET, ...) of this address */
		int protocol;           /*!< the protocol of this address. This can be used to distinguish between different types of an address family. */
	// #ifdef HAVE_SOCKLEN_T
	#ifndef _WIN32
		socklen_t len;          /*!< the length of the socket address */
	#else
	   int len;
	#endif
		union socket_addresses_u address; /* the socket address */
	};

	typedef struct rs232_network_socket_address_s rs232_network_socket_address_t;

	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	FileOperations::FILE_t inFile;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue
	Poller poller;

	std::ofstream outFile;

	StringSetting rs232NetAddressSetting;

	BooleanSetting rs232NetUseIP232;

	SOCKET sockfd;

	rs232_network_socket_address_t socket_address;

};

} // namespace openmsx

#endif
