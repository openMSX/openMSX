#ifndef RS232NET_HH
#define RS232NET_HH

#include "RS232Device.hh"

#include "EventListener.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "Socket.hh"

#include "Poller.hh"
#include "circular_buffer.hh"

#include <atomic>
#include <mutex>
#include <optional>
#include <span>
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
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// input
	void signal(EmuTime time) override;

	// output
	void recvByte(uint8_t value, EmuTime time) override;

	[[nodiscard]] std::optional<bool> getDSR(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getCTS(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getDCD(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getRI(EmuTime time) const override;
	void setDTR(bool status, EmuTime time) override;
	void setRTS(bool status, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run(); // loop of helper thread that reads from 'sockfd'

	// EventListener
	bool signalEvent(const Event& event) override;

	bool net_put(std::span<const char> buf);
	void open_socket(const NetworkSocketAddress& socket_address);

private:
	[[no_unique_address]] SocketActivator socketActivator;
	EventDistributor& eventDistributor;
	Scheduler& scheduler;

	// The value of these settings only matters at the time this device gets plugged in.
	// In other words: changing these settings only takes effect the next time the device gets plugged in.
	StringSetting rs232NetAddressSetting;
	BooleanSetting rs232NetUseIP232;

	std::thread thread; // receiving thread (reads from 'sockfd')
	std::mutex mutex; // to protect shared data between emulation and receiving thread
	std::optional<Poller> poller; // safe to use from main and receiver thread without extra locking
	cb_queue<char> queue; // read/written by both the main and the receiver thread. Must hold 'mutex' while doing so.
	std::atomic<SOCKET> sockfd; // read/written by both threads (use std::atomic as an alternative for locking)

	// These are written by the receiver thread and read by the main thread (use std::atomic as an alternative for locking)
	std::atomic<bool> DCD; // Data Carrier Detect input status
	std::atomic<bool> RI;  // Ring Indicator input status    TODO not yet used (write-only)

	// These are only accessed by the main thread (no need for locking)
	bool DTR; // Data Terminal Ready output status
	bool RTS; // Request To Send output status

	// This does not change while the receiving thread is running (no need for locking)
	bool IP232; // snapshot of 'rs232NetUseIP232' at the moment of plugging
};

} // namespace openmsx

#endif
