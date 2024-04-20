#include "MidiSessionALSA.hh"
#include "CliComm.hh"
#include "MidiOutDevice.hh"
#include "MidiInDevice.hh"
#include "MidiInConnector.hh"
#include "EventListener.hh"
#include "EventDistributor.hh"
#include "PlugException.hh"
#include "PluggingController.hh"
#include "Scheduler.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "circular_buffer.hh"
#include "checked_cast.hh"
#include <iostream>
#include <memory>
#include <thread>


namespace openmsx {

// MidiOutALSA ==============================================================

class MidiOutALSA final : public MidiOutDevice {
public:
	MidiOutALSA(snd_seq_t& seq,
	            snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo);
	MidiOutALSA(const MidiOutALSA&) = delete;
	MidiOutALSA(MidiOutALSA&&) = delete;
	MidiOutALSA& operator=(const MidiOutALSA&) = delete;
	MidiOutALSA& operator=(MidiOutALSA&&) = delete;
	~MidiOutALSA() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiOutDevice
	void recvMessage(
			const std::vector<uint8_t>& message, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void connect();
	void disconnect();

private:
	snd_seq_t& seq;
	snd_midi_event_t* event_parser;
	int sourcePort = -1;
	int destClient;
	int destPort;
	std::string name;
	std::string desc;
	bool connected = false;
};

MidiOutALSA::MidiOutALSA(
		snd_seq_t& seq_,
		snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo)
	: seq(seq_)
	, destClient(snd_seq_port_info_get_client(&pinfo))
	, destPort(snd_seq_port_info_get_port(&pinfo))
	, name(snd_seq_client_info_get_name(&cinfo))
	, desc(snd_seq_port_info_get_name(&pinfo))
{
}

MidiOutALSA::~MidiOutALSA()
{
	if (connected) {
		disconnect();
	}
}

void MidiOutALSA::plugHelper(Connector& /*connector_*/, EmuTime::param /*time*/)
{
	connect();
}

void MidiOutALSA::unplugHelper(EmuTime::param /*time*/)
{
	disconnect();
}

void MidiOutALSA::connect()
{
	sourcePort = snd_seq_create_simple_port(
		&seq, "MIDI out pluggable",
		0, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if (sourcePort < 0) {
		throw PlugException(
			"Failed to create ALSA port: ", snd_strerror(sourcePort));
	}

	if (int err = snd_seq_connect_to(&seq, sourcePort, destClient, destPort)) {
		snd_seq_delete_simple_port(&seq, sourcePort);
		throw PlugException(
			"Failed to connect to ALSA port "
			"(", destClient, ':', destPort, ")"
			": ", snd_strerror(err));
	}

	snd_midi_event_new(MAX_MESSAGE_SIZE, &event_parser);

	connected = true;
}

void MidiOutALSA::disconnect()
{
	snd_midi_event_free(event_parser);
	snd_seq_disconnect_to(&seq, sourcePort, destClient, destPort);
	snd_seq_delete_simple_port(&seq, sourcePort);

	connected = false;
}

std::string_view MidiOutALSA::getName() const
{
	return name;
}

std::string_view MidiOutALSA::getDescription() const
{
	return desc;
}

void MidiOutALSA::recvMessage(
		const std::vector<uint8_t>& message, EmuTime::param /*time*/)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Set routing.
	snd_seq_ev_set_source(&ev, narrow_cast<uint8_t>(sourcePort));
	snd_seq_ev_set_subs(&ev);

	// Set message.
	if (auto encodeLen = snd_midi_event_encode(
			event_parser, message.data(), narrow<long>(message.size()), &ev);
	    encodeLen < 0) {
		std::cerr << "Error encoding MIDI message of type "
		          << std::hex << int(message[0]) << std::dec
		          << ": " << snd_strerror(narrow<int>(encodeLen)) << '\n';
		return;
	}
	if (ev.type == SND_SEQ_EVENT_NONE) {
		std::cerr << "Incomplete MIDI message of type "
		          << std::hex << int(message[0]) << std::dec << '\n';
		return;
	}

	// Send event.
	snd_seq_ev_set_direct(&ev);
	if (int err = snd_seq_event_output(&seq, &ev); err < 0) {
		std::cerr << "Error sending MIDI event: "
		          << snd_strerror(err) << '\n';
	}
	snd_seq_drain_output(&seq);
}

template<typename Archive>
void MidiOutALSA::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	if constexpr (Archive::IS_LOADER) {
		connect();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutALSA);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutALSA, "MidiOutALSA");

// MidiInALSA ==============================================================
class MidiInALSA final : public MidiInDevice, private EventListener
{
public:
	MidiInALSA(
			EventDistributor& eventDistributor, Scheduler& scheduler, snd_seq_t& seq,
			snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo);
	~MidiInALSA() override;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;

	// MidiInDevice
	void signal(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void run();
	void connect();
	void disconnect();

	// EventListener
	int signalEvent(const Event& event) override;

private:
	EventDistributor& eventDistributor;
	Scheduler& scheduler;
	std::thread thread;
	snd_seq_t& seq;
	snd_midi_event_t* event_parser;
	int destinationPort = -1;
	int srcClient;
	int srcPort;
	std::string name;
	std::string desc;
	bool connected = false;
	bool stop = false;
	cb_queue<uint8_t> queue;
	std::mutex mutex; // to protect queue
};

MidiInALSA::MidiInALSA(
		EventDistributor& eventDistributor_,
		Scheduler& scheduler_,
		snd_seq_t& seq_,
		snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo)
	: eventDistributor(eventDistributor_)
	, scheduler(scheduler_)
	, seq(seq_)
	, srcClient(snd_seq_port_info_get_client(&pinfo))
	, srcPort(snd_seq_port_info_get_port(&pinfo))
	, name(snd_seq_client_info_get_name(&cinfo))
	, desc(snd_seq_port_info_get_name(&pinfo))
{
	eventDistributor.registerEventListener(EventType::MIDI_IN_ALSA, *this);
	if (snd_seq_port_info_get_capability(&pinfo) & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) {
		name.append(" (input)");
	}
}

MidiInALSA::~MidiInALSA()
{
	if (connected) {
		disconnect();
	}
	eventDistributor.unregisterEventListener(EventType::MIDI_IN_ALSA, *this);
}

void MidiInALSA::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	auto& midiConnector = checked_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&midiConnector); // base class will do this in a moment,
	                              // but thread already needs it
	connect();
}

void MidiInALSA::unplugHelper(EmuTime::param /*time*/)
{
	if (connected) {
		disconnect();
	}
}

void MidiInALSA::connect()
{
	destinationPort = snd_seq_create_simple_port(
		&seq, "MIDI in pluggable",
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if (destinationPort < 0) {
		throw PlugException(
			"Failed to create ALSA port: ", snd_strerror(destinationPort));
	}

	if (int err = snd_seq_connect_from(&seq, destinationPort, srcClient, srcPort)) {
		snd_seq_delete_simple_port(&seq, destinationPort);
		throw PlugException(
			"Failed to connect to ALSA port "
			"(", srcClient, ':', srcPort, ")"
			": ", snd_strerror(err));
	}

	snd_midi_event_new(MidiOutDevice::MAX_MESSAGE_SIZE, &event_parser);
	snd_midi_event_no_status(event_parser, 1);

	connected = true;
	stop = false;

	thread = std::thread([this]() { run(); });
}

void MidiInALSA::disconnect()
{
	stop = true;
	thread.join();

	snd_midi_event_free(event_parser);
	snd_seq_disconnect_from(&seq, destinationPort, srcClient, srcPort);
	snd_seq_delete_simple_port(&seq, destinationPort);

	connected = false;
}

void MidiInALSA::run()
{
	assert(isPluggedIn());

	auto npfd = snd_seq_poll_descriptors_count(&seq, POLLIN);
	std::vector<struct pollfd> pfd(npfd);
	snd_seq_poll_descriptors(&seq, pfd.data(), npfd, POLLIN);

	while (!stop) {
		if (poll(pfd.data(), npfd, 1000) > 0) {
			snd_seq_event_t *ev = nullptr;
			if (auto err = snd_seq_event_input(&seq, &ev); err < 0) {
				std::cerr << "Error receiving MIDI event: "
					<< snd_strerror(err) << '\n';
				continue;
			}
			if (!ev) continue;

			if (ev->type == SND_SEQ_EVENT_SYSEX) {
				std::scoped_lock lock(mutex);
				for (auto i : xrange(ev->data.ext.len)) {
					queue.push_back(static_cast<uint8_t*>(ev->data.ext.ptr)[i]);
				}

			} else {
				std::array<uint8_t, 12> bytes = {};
				auto size = snd_midi_event_decode(event_parser, bytes.data(), bytes.size(), ev);
				if (size < 0) {
					std::cerr << "Error decoding MIDI event: "
					          << snd_strerror(int(size)) << '\n';
					snd_seq_free_event(ev);
					continue;
				}

				std::scoped_lock lock(mutex);
				for (auto i : xrange(size)) {
					queue.push_back(bytes[i]);
				}
			}
			snd_seq_free_event(ev);
			eventDistributor.distributeEvent(MidiInALSAEvent());
		}
	}
}

void MidiInALSA::signal(EmuTime::param time)
{
	auto* conn = checked_cast<MidiInConnector*>(getConnector());
	if (!conn->acceptsData()) {
		std::scoped_lock lock(mutex);
		queue.clear();
		return;
	}
	if (!conn->ready()) return;

	uint8_t data;
	{
		std::scoped_lock lock(mutex);
		if (queue.empty()) return;
		data = queue.pop_front();
	}
	conn->recvByte(data, time);
}

// EventListener
int MidiInALSA::signalEvent(const Event& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		std::scoped_lock lock(mutex);
		queue.clear();
	}
	return 0;
}

std::string_view MidiInALSA::getName() const
{
	return name;
}

std::string_view MidiInALSA::getDescription() const
{
	return desc;
}

template<typename Archive>
void MidiInALSA::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	if constexpr (Archive::IS_LOADER) {
		connect();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MidiInALSA);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInALSA, "MidiInALSA");


// MidiSessionALSA ==========================================================

std::unique_ptr<MidiSessionALSA> MidiSessionALSA::instance;

void MidiSessionALSA::registerAll(
		PluggingController& controller, CliComm& cliComm,
		EventDistributor& eventDistributor, Scheduler& scheduler)
{
	if (!instance) {
		// Open the sequencer.
		snd_seq_t* seq;
		if (int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
		    err < 0) {
			cliComm.printError(
				"Could not open sequencer: ", snd_strerror(err));
			return;
		}
		snd_seq_set_client_name(seq, "openMSX");
		instance.reset(new MidiSessionALSA(*seq));
	}
	instance->scanClients(controller, eventDistributor, scheduler);
}

MidiSessionALSA::MidiSessionALSA(snd_seq_t& seq_)
	: seq(seq_)
{
}

MidiSessionALSA::~MidiSessionALSA()
{
	// While the Pluggables still have a copy of this pointer, they won't
	// be accessing it anymore when openMSX is exiting.
	snd_seq_close(&seq);
}

void MidiSessionALSA::scanClients(
	PluggingController& controller,
	EventDistributor& eventDistributor,
	Scheduler& scheduler)
{
	// Iterate through all clients.
	snd_seq_client_info_t* cInfo;
	snd_seq_client_info_alloca(&cInfo);
	snd_seq_client_info_set_client(cInfo, -1);
	while (snd_seq_query_next_client(&seq, cInfo) >= 0) {
		int client = snd_seq_client_info_get_client(cInfo);
		if (client == SND_SEQ_CLIENT_SYSTEM) {
			continue;
		}

		// TODO: When there is more than one usable port per client,
		//       register them as separate pluggables.
		snd_seq_port_info_t* pInfo;
		snd_seq_port_info_alloca(&pInfo);
		snd_seq_port_info_set_client(pInfo, client);
		snd_seq_port_info_set_port(pInfo, -1);
		while (snd_seq_query_next_port(&seq, pInfo) >= 0) {
			if (unsigned type = snd_seq_port_info_get_type(pInfo);
			    !(type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
				continue;
			}
			constexpr unsigned wrCaps =
					SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
			constexpr unsigned rdCaps =
					SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
			if ((snd_seq_port_info_get_capability(pInfo) & wrCaps) == wrCaps) {
				controller.registerPluggable(std::make_unique<MidiOutALSA>(
						seq, *cInfo, *pInfo));
			}
			if ((snd_seq_port_info_get_capability(pInfo) & rdCaps) == rdCaps) {
				controller.registerPluggable(std::make_unique<MidiInALSA>(
						eventDistributor, scheduler, seq, *cInfo, *pInfo));
			}
		}
	}
}

} // namespace openmsx
