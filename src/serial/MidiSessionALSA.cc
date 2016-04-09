#include "MidiSessionALSA.hh"
#include "CliComm.hh"
#include "MidiOutDevice.hh"
#include "PlugException.hh"
#include "PluggingController.hh"
#include "StringOp.hh"
#include "serialize.hh"

#include <iostream>


namespace openmsx {

// MidiOutALSA ==============================================================

class MidiOutALSA final : public MidiOutDevice {
public:
	MidiOutALSA(
			snd_seq_t& seq,
			snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo);
	~MidiOutALSA();

	// Pluggable
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
	const std::string& getName() const override;
	string_ref getDescription() const override;

	// MidiOutDevice
	void recvMessage(
			const std::vector<uint8_t>& message, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void connect();
	void disconnect();

	snd_seq_t& seq;
	snd_midi_event_t* event_parser;
	int sourcePort;
	int destClient;
	int destPort;
	std::string name;
	std::string desc;
	bool connected;
};

MidiOutALSA::MidiOutALSA(
		snd_seq_t& seq_,
		snd_seq_client_info_t& cinfo, snd_seq_port_info_t& pinfo)
	: seq(seq_)
	, sourcePort(-1)
	, destClient(snd_seq_port_info_get_client(&pinfo))
	, destPort(snd_seq_port_info_get_port(&pinfo))
	, name(snd_seq_client_info_get_name(&cinfo))
	, desc(snd_seq_port_info_get_name(&pinfo))
	, connected(false)
{
}

MidiOutALSA::~MidiOutALSA() {
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
		throw PlugException(StringOp::Builder() <<
				"Failed to create ALSA port: " << snd_strerror(sourcePort));
	}

	int err = snd_seq_connect_to(&seq, sourcePort, destClient, destPort);
	if (err) {
		snd_seq_delete_simple_port(&seq, sourcePort);
		throw PlugException(StringOp::Builder() <<
				"Failed to connect to ALSA port "
				"(" << destClient << ":" << destPort << ")"
				": " << snd_strerror(err));
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

const std::string& MidiOutALSA::getName() const
{
	return name;
}

string_ref MidiOutALSA::getDescription() const
{
	return desc;
}

void MidiOutALSA::recvMessage(
		const std::vector<uint8_t>& message, EmuTime::param /*time*/)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Set routing.
	snd_seq_ev_set_source(&ev, sourcePort);
	snd_seq_ev_set_subs(&ev);

	// Set message.
	long encodeLen = snd_midi_event_encode(
			event_parser, message.data(), message.size(), &ev);
	if (encodeLen < 0) {
		std::cerr << "Error encoding MIDI message of type "
		          << std::hex << int(message[0]) << std::dec
		          << ": " << snd_strerror(encodeLen) << std::endl;
		return;
	}
	if (ev.type == SND_SEQ_EVENT_NONE) {
		std::cerr << "Incomplete MIDI message of type "
		          << std::hex << int(message[0]) << std::dec << std::endl;
		return;
	}

	// Send event.
	snd_seq_ev_set_direct(&ev);
	int err = snd_seq_event_output(&seq, &ev);
	if (err < 0) {
		std::cerr << "Error sending MIDI event: "
		          << snd_strerror(err) << std::endl;
	}
	snd_seq_drain_output(&seq);
}

template<typename Archive>
void MidiOutALSA::serialize(Archive& ar, unsigned /*version*/)
{
	if (ar.isLoader()) {
		connect();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutALSA);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutALSA, "MidiOutALSA");


// MidiSessionALSA ==========================================================

std::unique_ptr<MidiSessionALSA> MidiSessionALSA::instance;

void MidiSessionALSA::registerAll(
		PluggingController& controller, CliComm& cliComm)
{
	if (!instance) {
		// Open the sequencer.
		snd_seq_t* seq;
		int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
		if (err < 0) {
			cliComm.printError(StringOp::Builder() <<
					"Could not open sequencer: " << snd_strerror(err));
			return;
		}
		snd_seq_set_client_name(seq, "openMSX");
		instance.reset(new MidiSessionALSA(*seq));
	}
	instance->scanClients(controller);
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

void MidiSessionALSA::scanClients(PluggingController& controller)
{
	// Iterate through all clients.
	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(&seq, cinfo) >= 0) {
		int client = snd_seq_client_info_get_client(cinfo);
		if (client == SND_SEQ_CLIENT_SYSTEM) {
			continue;
		}

		// TODO: When there is more than one usable port per client,
		//       register them as separate pluggables.
		snd_seq_port_info_t* pinfo;
		snd_seq_port_info_alloca(&pinfo);
		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(&seq, pinfo) >= 0) {
			unsigned int type = snd_seq_port_info_get_type(pinfo);
			if (!(type & SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
				continue;
			}
			constexpr unsigned int wrcaps =
					SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
			if ((snd_seq_port_info_get_capability(pinfo) & wrcaps) == wrcaps) {
				controller.registerPluggable(make_unique<MidiOutALSA>(
						seq, *cinfo, *pinfo
						));
			}
		}
	}
}

} // namespace openmsx
