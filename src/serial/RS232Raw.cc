// Keep these before "RS232Raw.hh": that header pulls in <windows.h> (via
// SerialPort.hh), whose macros (TRANSPARENT, BLACK, WHITE, YELLOW, ...) would
// otherwise clash with the imColor enum in ImGuiUtils.hh.
#include "ImGuiCpp.hh"
#include "ImGuiUtils.hh"

#include "RS232Raw.hh"

#include "RS232Connector.hh"

#include "CliComm.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "PlugException.hh"
#include "Scheduler.hh"
#include "serialize.hh"

#include "checked_cast.hh"

#include <imgui.h>

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace openmsx {

RS232Raw::RS232Raw(EventDistributor& eventDistributor_,
                   Scheduler& scheduler_,
                   CommandController& commandController)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, cliComm(commandController.getCliComm())
	, rs232RawPortSetting(
	        commandController, "rs232-raw-port",
	        "Serial port name for RS232 raw pluggable",
#ifdef _WIN32
	        "COM1"
#else
	        "/dev/ttyS0"
#endif
        )
{
	eventDistributor.registerEventListener(EventType::RS232_RAW, *this);
	rs232RawPortSetting.attach(*this);
}

RS232Raw::~RS232Raw()
{
	rs232RawPortSetting.detach(*this);
	eventDistributor.unregisterEventListener(EventType::RS232_RAW, *this);
}

static serial::SerialParams buildParamsImpl(unsigned baud, SerialDataInterface::DataBits dataBits, SerialDataInterface::StopBits stopBits, bool parityEnabled, SerialDataInterface::Parity parity)
{
	serial::SerialParams p;
	p.baudRate = baud;
	p.dataBits = static_cast<int>(dataBits);
	switch (stopBits) {
	using enum SerialDataInterface::StopBits;
	case S1:   p.stopBits = 1; break;
	case S1_5: p.stopBits = 1; break;
	case S2:   p.stopBits = 2; break;
	default:   p.stopBits = 1; break;
	}
	if (parityEnabled) {
		p.parity = (parity == SerialDataInterface::Parity::ODD) ? 1 : 2;
	} else {
		p.parity = 0;
	}
	return p;
}

void RS232Raw::applyParams()
{
	if (!handle && !thread.joinable()) return;

#ifdef _WIN32
	if (thread.joinable()) {
		poller->abort();
	}
	handle.reset();
	if (thread.joinable()) {
		thread.join();
	}
	{
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	auto portName = std::string(rs232RawPortSetting.getString().c_str());
	if (portName.empty()) return;

	auto p = buildParamsImpl(currentBaud, currentDataBits, currentStopBits,
	                         currentParityEnabled, currentParity);
	poller.reset();
	poller.emplace();
	thread = std::jthread([this, portName, p]() { openAndRun(portName, p); });
#else
	auto p = buildParamsImpl(currentBaud, currentDataBits, currentStopBits,
	                         currentParityEnabled, currentParity);
	(void)handle->set_params(p);
	(void)handle->set_dtr(DTR);
	(void)handle->set_rts(RTS);
#endif
}

// Pluggable
void RS232Raw::plugHelper(Connector& connector_, EmuTime /*time*/)
{
	auto portName = std::string(rs232RawPortSetting.getString().c_str());
	if (portName.empty()) {
		throw PlugException("No serial port specified");
	}

	auto& rs232Connector = checked_cast<RS232Connector&>(connector_);
	directConn = &rs232Connector;

	// Initialize the modem-line state BEFORE the worker opens the port, so
	// the lines are applied in their final state (DTR/RTS asserted) and never
	// toggle afterwards: a transition resets ESP32-style auto-reset circuits.
	DCD = false;
	RI  = false;
	CTS = false;
	DSR = true;
	DTR = true;
	RTS = true;

	auto params = buildParamsImpl(currentBaud, currentDataBits, currentStopBits,
	                              currentParityEnabled, currentParity);

	poller.emplace();
	thread = std::jthread([this, portName, params]() { openAndRun(portName, params); });

	rs232Connector.setDataBits(currentDataBits);
	rs232Connector.setStopBits(currentStopBits);
	rs232Connector.setParityBit(currentParityEnabled, currentParity);

	setConnector(&connector_);
}

void RS232Raw::unplugHelper(EmuTime /*time*/)
{
	if (thread.joinable()) {
		poller->abort();
	}
	handle.reset();
	if (thread.joinable()) {
		thread.join();
	}
	poller.reset();
	directConn = nullptr;
}

zstring_view RS232Raw::getName() const
{
	return "rs232-raw";
}

zstring_view RS232Raw::getDescription() const
{
	return "RS232 raw serial port pluggable. Connects the RS232 port to "
	       "a host serial port, selected with the 'rs232-raw-port' setting.";
}

void RS232Raw::handleImGuiExtraMenuItems()
{
	std::string cur(rs232RawPortSetting.getString().c_str());
	if (cur != currentPort) {
		ports = serial::list_ports();
		currentPort = cur;
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
	im::Combo("##rs232-raw-port", cur.c_str(), [this, &cur]{
		for (const auto& p : ports) {
			bool selected = (p == cur);
			if (ImGui::Selectable(p.c_str(), selected)) {
				rs232RawPortSetting.setString(p);
				currentPort = p;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
	});
	simpleToolTip("Select the host serial port for RS232 raw");
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh")) {
		ports = serial::list_ports();
	}
}

// Opens the port (retrying briefly) and then runs the read loop. Runs on
// the worker thread: the retry delays sleep this host thread, never the
// emulation thread (a just-closed Windows COM port can stay owned by the
// OS for a few ms, so an immediate reopen can transiently fail).
void RS232Raw::openAndRun(std::string portName, serial::SerialParams params)
{
	auto h = serial::open(portName, params);
	for (int attempt = 1; !h.has_value() && attempt < 5 && !poller->aborted();
	     ++attempt) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		h = serial::open(portName, params);
	}
	if (poller->aborted()) {
		return; // unplugged while opening: drop the port (h closes it)
	}
	if (!h.has_value()) {
		cliComm.printWarning("Failed to open serial port ", portName, ": ",
		                     serial::to_string(h.error()));
		return;
	}
	(void)h->set_params(params);
	(void)h->set_dtr(DTR);
	(void)h->set_rts(RTS);
	handle = std::move(*h);
	run();
}

void RS232Raw::run()
{
	auto* conn = directConn;
	bool direct = conn && conn->directByteDelivery();

	while (handle && !poller->aborted()) {
#ifndef _WIN32
		if (poller->poll(handle->native_handle())) {
			break;
		}
#endif

		char b;
		auto r = handle->read(std::span<char>(&b, 1));
		if (!r.has_value()) {
			handle.reset();
		} else if (*r == 0) {
#ifdef _WIN32
			Sleep(1);
#endif
		} else if (direct) {
			conn->recvByte(static_cast<uint8_t>(b), EmuTime::zero());
		} else {
			assert(isPluggedIn());
			{
				std::scoped_lock lock(mutex);
				queue.push_back(b);
			}
			eventDistributor.distributeEvent(Rs232RawEvent());
		}
	}
}

// input
void RS232Raw::signal(EmuTime time)
{
	if (!handle) return;

	auto* conn = checked_cast<RS232Connector*>(getConnector());

	if (!conn->acceptsData()) {
		return;
	}

	if (!conn->ready() || !RTS) return;

	std::scoped_lock lock(mutex);
	if (queue.empty()) return;
	char b = queue.pop_front();
	conn->recvByte(static_cast<uint8_t>(b), time);
}

// EventListener
bool RS232Raw::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return false;
}

// output
void RS232Raw::recvByte(uint8_t value, EmuTime /*time*/)
{
	if (!handle) return;

	auto buf = static_cast<char>(value);
	if (!handle->write(std::span<const char>(&buf, 1)).has_value()) {
		handle.reset();
	}
}

// Control lines

std::optional<bool> RS232Raw::getDSR(EmuTime /*time*/) const
{
	if (!handle) return {};
	return DSR.load();
}

std::optional<bool> RS232Raw::getCTS(EmuTime /*time*/) const
{
	if (!handle) return {};
	return CTS.load();
}

std::optional<bool> RS232Raw::getDCD(EmuTime /*time*/) const
{
	if (!handle) return {};
	return DCD.load();
}

std::optional<bool> RS232Raw::getRI(EmuTime /*time*/) const
{
	if (!handle) return {};
	return RI.load();
}

void RS232Raw::setDTR(bool status, EmuTime /*time*/)
{
	if (DTR == status) return;
	DTR = status;
	if (handle) {
		(void)handle->set_dtr(status);
	}
}

void RS232Raw::setRTS(bool status, EmuTime /*time*/)
{
	if (RTS == status) return;
	RTS = status;
	if (handle) {
		(void)handle->set_rts(status);
		if (RTS) {
			std::scoped_lock lock(mutex);
			if (!queue.empty()) {
				eventDistributor.distributeEvent(Rs232RawEvent());
			}
		}
	}
}

// Serial params

void RS232Raw::setDataBits(DataBits bits)
{
	if (currentDataBits.load() == bits) return;
	currentDataBits.store(bits);
	applyParams();
}

void RS232Raw::setStopBits(StopBits bits)
{
	if (currentStopBits.load() == bits) return;
	currentStopBits.store(bits);
	applyParams();
}

void RS232Raw::setParityBit(bool enable, Parity parity)
{
	if (currentParityEnabled.load() == enable && currentParity.load() == parity) return;
	currentParityEnabled.store(enable);
	currentParity.store(parity);
	applyParams();
}

void RS232Raw::setBaudRate(unsigned baud)
{
	if (currentBaud.load() == baud) return;
	currentBaud.store(baud);
	applyParams();
}

void RS232Raw::update(const Setting& /*setting*/) noexcept
{
	if (thread.joinable()) {
		poller->abort();
	}
	handle.reset();
	if (thread.joinable()) {
		thread.join();
	}
	poller.reset();
	{
		std::scoped_lock lock(mutex);
		queue.clear();
	}

	auto portName = std::string(rs232RawPortSetting.getString().c_str());
	if (!portName.empty() && isPluggedIn()) {
		auto p = buildParamsImpl(currentBaud, currentDataBits, currentStopBits,
		                         currentParityEnabled, currentParity);
		poller.emplace();
		thread = std::jthread([this, portName, p]() { openAndRun(portName, p); });
	}
}

template<typename Archive>
void RS232Raw::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	if constexpr (Archive::IS_LOADER) {
		if (!isPluggedIn()) return;

		auto* conn = dynamic_cast<RS232Connector*>(getConnector());
		if (!conn) return;
		directConn = conn;

		auto portName = std::string(rs232RawPortSetting.getString().c_str());
		if (portName.empty()) return;

		auto params = buildParamsImpl(currentBaud.load(), currentDataBits.load(),
		                              currentStopBits.load(), currentParityEnabled.load(),
		                              currentParity.load());
		poller.emplace();
		thread = std::jthread([this, portName, params]() { openAndRun(portName, params); });
	}
}
INSTANTIATE_SERIALIZE_METHODS(RS232Raw);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, RS232Raw, "RS232Raw");

} // namespace openmsx
