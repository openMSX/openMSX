#ifndef RS232RAW_HH
#define RS232RAW_HH

#include "RS232Device.hh"

#include "EventListener.hh"
#include "Observer.hh"
#include "StringSetting.hh"
#include "SerialPort.hh"
#include "ImGuiPluggable.hh"

#include "Poller.hh"
#include "circular_buffer.hh"
#include "zstring_view.hh"

#include <atomic>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <cstdint>

namespace openmsx {

class CliComm;
class EventDistributor;
class RS232Connector;
class Scheduler;
class CommandController;

class RS232Raw final : public RS232Device, public ImGuiPluggable,
                       private EventListener, private Observer<Setting>
{
public:
	RS232Raw(EventDistributor& eventDistributor, Scheduler& scheduler,
	         CommandController& commandController);
	~RS232Raw() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;

	// ImGuiPluggable
	void renderGuiExtra() override;

	// input
	void signal(EmuTime time) override;

	// output
	void recvByte(uint8_t value, EmuTime time) override;

	// control lines
	[[nodiscard]] std::optional<bool> getDSR(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getCTS(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getDCD(EmuTime time) const override;
	[[nodiscard]] std::optional<bool> getRI(EmuTime time) const override;
	void setDTR(bool status, EmuTime time) override;
	void setRTS(bool status, EmuTime time) override;

	// serial params
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, Parity parity) override;
	void setBaudRate(unsigned baud) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();
	void applyParams();

	// EventListener
	bool signalEvent(const Event& event) override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	CliComm& cliComm;

	StringSetting rs232RawPortSetting;

	serial_handle_t handle;
	std::thread thread;
	std::mutex mutex;
	std::optional<Poller> poller;
	cb_queue<char> queue;

	std::atomic<bool> DCD;
	std::atomic<bool> RI;
	std::atomic<bool> CTS;
	std::atomic<bool> DSR;

	bool DTR;
	bool RTS;
	RS232Connector* directConn = nullptr;

	std::atomic<unsigned> currentBaud;
	std::atomic<DataBits> currentDataBits;
	std::atomic<StopBits> currentStopBits;
	std::atomic<Parity> currentParity;
	std::atomic<bool> currentParityEnabled;
};

} // namespace openmsx

#endif
