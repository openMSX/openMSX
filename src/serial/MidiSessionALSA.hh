#ifndef MIDISESSIONALSA_HH
#define MIDISESSIONALSA_HH

#include <alsa/asoundlib.h>
#include <memory>


namespace openmsx {

class CliComm;
class EventDistributor;
class Scheduler;
class PluggingController;


/** Lists ALSA MIDI ports we can connect to.
  */
class MidiSessionALSA final
{
public:
	static void registerAll(PluggingController& controller, CliComm& cliComm,
	                        EventDistributor& eventDistributor, Scheduler& scheduler);

	MidiSessionALSA(const MidiSessionALSA&) = delete;
	MidiSessionALSA(MidiSessionALSA&&) = delete;
	MidiSessionALSA& operator=(const MidiSessionALSA&) = delete;
	MidiSessionALSA& operator=(MidiSessionALSA&&) = delete;
	~MidiSessionALSA();

private:
	static std::unique_ptr<MidiSessionALSA> instance;

	explicit MidiSessionALSA(snd_seq_t& seq);
	void scanClients(PluggingController& controller,
	                 EventDistributor& eventDistributor,
	                 Scheduler& scheduler);

	snd_seq_t& seq;
};

} // namespace openmsx

#endif // MIDISESSIONALSA_HH
