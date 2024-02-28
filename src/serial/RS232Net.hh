#ifndef RS232NET_HH
#define RS232NET_HH

#include "RS232Device.hh"

#include "EventListener.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "Socket.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

#include <mutex>
#include <thread>
#include <cstdint>

namespace openmsx {

class EventDistributor;
class Scheduler;
class CommandController;

class RS232Net final : public RS232Device, private EventListener
{
public:
	// opaque structure describing an address for use with socket functions
	struct NetworkSocketAddress {
		int domain; // the address family (AF_INET, ...)
		socklen_t len; // the length of the socket address
		union {
			struct sockaddr generic;  // the generic type needed for calling the socket API
			struct sockaddr_in ipv4;  // an IPv4 socket address
			struct sockaddr_in6 ipv6; // an IPv6 socket address
		} address;
	};

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

	[[nodiscard]] bool getDCD(EmuTime::param time) const override;
	void setDTR(bool status, EmuTime::param time) override;
	void setRTS(bool status, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();

	bool DTR; // Data Terminal Ready output status
	bool RTS; // Request To Send output status
	bool DCD; // Data Carrier Detect input status
	bool RI;  // Ring Indicator input status
	bool IP232;

	// EventListener
	int signalEvent(const Event& event) override;

	bool net_putc(char b);
	[[nodiscard]] std::optional<char> net_getc();
	[[nodiscard]] int selectPoll(SOCKET readSock);
	void open_socket(const NetworkSocketAddress& socket_address);

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	cb_queue<char> queue;
	std::mutex mutex; // to protect queue
	Poller poller;

	StringSetting rs232NetAddressSetting;
	BooleanSetting rs232NetUseIP232;

	SOCKET sockfd;
};

} // namespace openmsx

#endif
